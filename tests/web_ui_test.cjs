/* Execute the production browser script with a small DOM/transport harness.
 * This verifies interaction and timing; it is not a browser layout screenshot. */
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const embedded=fs.readFileSync(__dirname+'/../src/http_page.h','utf8').split('static const char http_page[]=\n')[1].trim();
const html=JSON.parse(embedded.slice(0,-1)),script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
const timers=new Map(),callbacks=new Map(),requests=[],assistantRequests=[],speechRequests=[],responses=[],draws=[];let counter=0,clock=0,active=0,maxActive=0;
class Element{
 constructor(){this.children=[];this.handlers={};this.value='';this.disabled=false;this.scrollHeight=500;}
 append(...items){this.children.push(...items);}
 replaceChildren(){this.children=[];}
 addEventListener(name,fn){this.handlers[name]=fn;}
 setAttribute(name,value){this[name]=value;}
 get firstChild(){return {remove:()=>this.children.shift()};}
 getBoundingClientRect(){return {width:640,height:480};}
 getContext(){return {fillRect(){},drawImage(...args){draws.push(args);}};}
 focus(){this.focused=true;}
}
const ids=['log','message','send','reset','connection','face','form','mic','stop','voice-on','voice','offline','voice-note','auto-send','hands-free','notice','thinking','thinking-label','thinking-elapsed','thinking-bar'];
const elements=Object.fromEntries(ids.map(id=>[id,new Element()]));
const motion={matches:false,addEventListener(name,fn){this.handler=fn;}};
const utterances=[];let cancelled=0,speechOwner='',pendingAlarm='',newsResponse='pending|',newsStatus=200,searchResponse='pending|',searchStatus=200,speechFailures=0;
const synthesis={getVoices:()=>[{name:'Microsoft George',lang:'en-GB',voiceURI:'george',localService:true}],cancel(){cancelled++;},speak(u){utterances.push(u);},addEventListener(){}};
class RecognitionMock{constructor(){this.processLocally=false;}start(){this.onstart();}abort(){this.aborted=true;}}
const sandbox={document:{getElementById:id=>elements[id],createElement:()=>new Element(),createTextNode:text=>({textContent:text})},SpeechSynthesisUtterance:class{constructor(text){this.text=text;}},window:{isSecureContext:true,speechSynthesis:synthesis,webkitSpeechRecognition:RecognitionMock,devicePixelRatio:1,matchMedia:()=>motion,addEventListener(){}},Image:class{constructor(){this.naturalWidth=512;this.naturalHeight=6048;}},performance:{now:()=>clock},AbortController,setTimeout:(fn,delay)=>{let id=++counter;timers.set(id,{fn,at:clock+delay});return id;},clearTimeout:id=>timers.delete(id),requestAnimationFrame:fn=>{let id=++counter;callbacks.set(id,fn);return id;},cancelAnimationFrame:id=>callbacks.delete(id),fetch:async(path,opts)=>{if(path==='/chat')assert.equal(elements.thinking.hidden,false);active++;maxActive=Math.max(maxActive,active);if(path==='/clock'||path==='/alerts'||path==='/news'||path==='/search'){assistantRequests.push({path,opts});await Promise.resolve();return {ok:path==='/news'?newsStatus===200:path==='/search'?searchStatus===200:true,status:path==='/news'?newsStatus:path==='/search'?searchStatus:200,text:async()=>{active--;return path==='/clock'?'Clock synchronised.':path==='/news'?newsResponse:path==='/search'?searchResponse:opts.body==='poll'?pendingAlarm:'';}};}if(path==='/speech'){speechRequests.push({path,opts});const owner=opts.headers['X-Holly-Session'];let status=200;if(speechFailures){speechFailures--;status=503;}else if(/^begin /.test(opts.body))speechOwner=owner;else if(/^end /.test(opts.body)){if(speechOwner===owner)speechOwner='';}else if(speechOwner!==owner)status=409;await Promise.resolve();return {ok:status===200,status,text:async()=>{active--;return 'OK';}};}requests.push({path,opts});await Promise.resolve();const r=responses.shift();assert(r,'Unexpected request');return {...r,text:async()=>{active--;return r.text;}};}};
vm.createContext(sandbox);vm.runInContext(script,sandbox);
const run=code=>vm.runInContext(code,sandbox);
function tick(time){clock=time;for(const [id,timer] of [...timers])if(timer.at<=clock){timers.delete(id);timer.fn();}const pending=[...callbacks.values()];callbacks.clear();for(const fn of pending)fn(clock);}
(async()=>{
 assert(html.includes('HollyOS V0.49.29')&&html.includes('maxlength="319"'));assert(!html.includes('type="checkbox"')&&!html.includes('<select')&&!html.includes('id="log"'));run('voiceOn.checked=false');
 assert.deepEqual(Array.from(run("mouthEvents('aoiembpt').map(x=>x.frame)")),[3,4,2,5,0,0,0,6]);
 assert.equal(run("mouthEvents('a'.repeat(4000)).length"),128);
 run('artReady=true;paint(0)');assert.equal(draws.at(-1)[2],0);assert(draws.at(-1)[7]<=640&&draws.at(-1)[8]<=480);
 responses.push({ok:true,status:200,text:'a'.repeat(32)});await run('connect()');assert.equal(run('token'),'a'.repeat(32));assert.equal(assistantRequests[0].path,'/clock');assert.match(assistantRequests[0].opts.body,/^\d+ -?\d+$/);assert.equal(elements.send.disabled,false);assert.equal(elements.thinking.hidden,true);
 responses.push({ok:true,status:200,text:'Holly: Aaaa.'});const promise=run("chat('Who is Holly?')");await run("chat('duplicate')");await promise;
 assert.equal(elements.thinking.hidden,true);assert.equal(requests.length,2);assert.equal(requests[1].path,'/chat');assert.equal(requests[1].opts.body,'Who is Holly?');
 assert.equal(run('log').children.at(-1).children[1].textContent,'Aaaa.');
 tick(0);assert.equal(run('currentFrame'),3);tick(2000);assert.equal(run('currentFrame'),0);assert.equal(callbacks.size,0);
 run("talk('aaaa')");tick(2000);assert.equal(run('currentFrame'),3);run('stopTalking()');assert.equal(run('currentFrame'),0);assert.equal(callbacks.size,0);
 motion.matches=true;run("talk('aaa')");assert.equal(callbacks.size,0);motion.matches=false;
 await run("chat('é')");assert.equal(requests.length,2);
 responses.push({ok:false,status:410,text:'Session expired.'});await run("chat('hello')");assert.equal(run('token'),'');assert.equal(run('currentFrame'),0);assert.equal(elements.thinking.hidden,true);
 const before=requests.length;await run("chat('another')");assert.equal(requests.length,before);
 for(let i=0;i<150;i++)run("add('HOLLY','reply')");assert.equal(run('log').children.length,100);assert.equal(run('log').scrollTop,run('log').scrollHeight);
 responses.push({ok:true,status:200,text:'b'.repeat(32)});await run('connect()');assert.equal(run('log').children.length,1);
 for(let i=0;i<30;i++)await Promise.resolve();assert.equal(maxActive,1);assert.equal(active,0);
 const chunks=Array.from(run("captionChunks('A long response about Lister and Rimmer aboard Red Dwarf. Another sentence has enough words to need a second caption line.')"));
 assert(chunks.length>=2&&chunks.every(line=>line.length<=72));
 run("captionReply('First caption explains the crew aboard the ship. Second caption discusses another part of the story.')");
 assert.equal(run('captionActive'),true);assert(run('captionPending.length')>0);
 const shown=run('log').children.length;tick(clock+10000);assert(run('log').children.length>shown);
 run('finishCaptions()');assert.equal(run('captionActive'),false);assert.equal(run('captionPending.length'),0);assert.equal(callbacks.size,0);
 // Speech chunks follow completion, not a guessed timer.
 run('voiceOn.checked=true');
 run("voiceReply('First caption explains the crew aboard the ship. Second caption discusses another part of the story.')");
 assert.equal(utterances.length,1);assert.equal(utterances[0].voice.voiceURI,'george');
 utterances[0].onstart();utterances[0].onboundary({name:'word',charIndex:0});
 assert(run('captionPending.length')>0);utterances[0].onend();assert.equal(utterances.length,2);
 utterances[1].onend();assert.equal(run('speaking'),false);assert.equal(run('currentFrame'),0);
 run("voiceReply('A new reply to interrupt.')");const interrupted=utterances.at(-1);run('finishCaptions()');interrupted.onend();assert.equal(run('speaking'),false);assert.equal(run('captionPending.length'),0);
 elements.message.value='';await run('startListening()');assert.equal(run('listening'),true);
 run("recognition.onresult({resultIndex:0,results:[Object.assign([{transcript:'My name is Darren'}],{isFinal:true})]})");
 assert.equal(elements.message.value,'My name is Darren');assert.equal(requests.length,4); // microphone does not auto-send
 const oldRecognition=run('recognition');run('stopListening()');assert(oldRecognition.aborted);assert.equal(run('listening'),false);
 run('offline.checked=true');await run('startListening()');assert.equal(run('listening'),false);assert(elements.notice.textContent.includes('does not offer'));
 run('handsFreePaused=false');RecognitionMock.available=async()=> 'available';RecognitionMock.install=async()=>true;
 await run('startListening()');assert.equal(run('recognition.processLocally'),true);run('stopListening()');
 // Final speech auto-sends exactly once, errors/cancellation do not.
 run('offline.checked=false');run('autoSend.checked=true');elements.message.value='';
 await run('startListening()');responses.push({ok:true,status:200,text:'Holly: Hello Darren.'});
 await run("recognition.onresult({resultIndex:0,results:[Object.assign([{transcript:'Hello Holly'}],{isFinal:true})]});recognition.onend()");
 await Promise.resolve();await Promise.resolve();await Promise.resolve();await Promise.resolve();
 assert.equal(requests.at(-1).opts.body,'Hello Holly');assert.equal(run('listening'),false);
 run('finishCaptions()');elements.message.value='';await run('startListening()');const count=requests.length;
 run("recognition.onresult({resultIndex:0,results:[Object.assign([{transcript:'Do not send this'}],{isFinal:false})]});recognition.onend()");
 assert.equal(requests.length,count);elements.message.value='';await run('startListening()');
 run("recognition.onresult({resultIndex:0,results:[Object.assign([{transcript:'Failed speech'}],{isFinal:true})]});recognition.onerror({error:'network'});recognition.onend()");assert.equal(requests.length,count);
 elements.message.value='';await run('startListening()');run("recognition.onresult({resultIndex:0,results:[Object.assign([{transcript:'x'.repeat(400)}],{isFinal:true})]});recognition.onend()");assert.equal(requests.length,count);
 run('autoSend.checked=false');run('offline.checked=true');
 run('offline.checked=false');elements.message.value='';await run('startListening()');const beforeReview=requests.length;
 run("recognition.onresult({resultIndex:0,results:[Object.assign([{transcript:'Review this first'}],{isFinal:true})]});recognition.onend()");assert.equal(requests.length,beforeReview);assert.equal(elements.message.value,'Review this first');
 sandbox.window.isSecureContext=false;run('updateMic()');assert.equal(elements.mic.disabled,true);sandbox.window.isSecureContext=true;run('updateMic()');assert.equal(elements.mic.disabled,false);run('offline.checked=true');
 let resolveAvailability;RecognitionMock.available=()=>new Promise(resolve=>resolveAvailability=resolve);
 const pending=run('startListening()');run('stopListening()');resolveAvailability('available');await pending;assert.equal(run('recognition'),null);
 run('offline.checked=false');await run('startListening()');run('lock(true)');assert.equal(run('recognition'),null);run('lock(false)');
 run("voiceReply('A stuck voice must not keep animating forever.')");tick(clock+120000);assert.equal(run('speaking'),false);assert.equal(run('captionActive'),false);
 // HDMI follows the browser stream while speech is active.
 run('finishCaptions();input.value="";handsFree.checked=true;handsFreePaused=false');
 run("voiceReply('A long spoken reply must keep moving until speech ends.')");const live=utterances.at(-1);live.onstart();
 for(let i=0;i<30;i++)await Promise.resolve();
 assert(speechRequests.some(r=>/^begin /.test(r.opts.body)));
 run('paint(4)');tick(clock+500);for(let i=0;i<30;i++)await Promise.resolve();
 assert.equal(run('hdmiFrame'),run('currentFrame'));assert.equal(run('listening'),false);live.onend();
 for(let i=0;i<30;i++)await Promise.resolve();
 assert.equal(run('hdmiActive'),false);assert(run('listenTimer')>0);
 tick(clock+1000);await Promise.resolve();assert.equal(run('listening'),true);
 elements.mic.handlers.click();assert.equal(run('listening'),false);assert.equal(run('handsFree.checked'),false);
 for(let i=0;i<30;i++)await Promise.resolve();
 assert(speechRequests.some(r=>/^end /.test(r.opts.body)));
synthesis.getVoices=()=>[{name:'Microsoft George',lang:'en-GB',voiceURI:'george',localService:true},{name:'Google UK English Male',lang:'en-GB',voiceURI:'google-uk-male',localService:false}];run('loadVoices()');assert.equal(run('voiceSelect.value'),'google-uk-male');
synthesis.getVoices=()=>[{name:'Google UK English Male',lang:'en-GB',voiceURI:'google-uk-male',localService:false},{name:'Google UK English Female',lang:'en-GB',voiceURI:'google-uk-female',localService:false}];
 run("selectAvatar('hilly');paint(3)");assert.equal(run('voiceSelect.value'),'google-uk-female');assert.equal(draws.at(-1)[2],10*288);
 run("selectAvatar('holly')");assert.equal(run('voiceSelect.value'),'google-uk-male');
 responses.push({ok:true,status:200,text:'Holly: Hello.'});await run("chat('Hello HILLARY!')");assert.equal(run('avatar'),'hilly');
 responses.push({ok:true,status:200,text:'Holly: Another answer.'});await run("chat('Tell me about Rimmer')");assert.equal(run('avatar'),'hilly');
 responses.push({ok:true,status:200,text:'Holly: Same face.'});await run("chat('hillyard is not a trigger')");assert.equal(run('avatar'),'hilly');
 responses.push({ok:true,status:200,text:'Holly: Switched back.'});await run("chat('Hello HOLLY')");assert.equal(run('avatar'),'holly');run('finishCaptions()');
for(let i=0;i<40;i++)await Promise.resolve();const stopCount=speechRequests.filter(r=>/^end /.test(r.opts.body)).length;
 run('finishCaptions();finishCaptions();hdmiEnd()');for(let i=0;i<40;i++)await Promise.resolve();assert.equal(speechRequests.filter(r=>/^end /.test(r.opts.body)).length,stopCount);
 // Queeg atlas, voice, whole-word switching and thinking status.
 synthesis.getVoices=()=>[{name:'Google UK English Male',lang:'en-GB',voiceURI:'google-uk-male'},{name:'Google UK English Female',lang:'en-GB',voiceURI:'google-uk-female'},{name:'Google US English',lang:'en-US',voiceURI:'unspecified-us'},{name:'Microsoft Zira',lang:'en-US',voiceURI:'zira'},{name:'Microsoft David Desktop',lang:'en-US',voiceURI:'david-us',localService:true}];
 run("selectAvatar('queeg');paint(4)");assert.equal(draws.at(-1)[2],18*288);assert.equal(run('voiceSelect.value'),'david-us');
 run("voiceReply('Attend to the task.')");assert.equal(utterances.at(-1).voice.voiceURI,'david-us');assert.equal(utterances.at(-1).lang,'en-US');assert.equal(utterances.at(-1).pitch,.8);run('finishCaptions()');
 synthesis.getVoices=()=>[{name:'Google UK English Male',lang:'en-GB',voiceURI:'google-uk-male'},{name:'Google UK English Female',lang:'en-GB',voiceURI:'google-uk-female'},{name:'Google US English',lang:'en-US',voiceURI:'unspecified-us'},{name:'Microsoft Zira Female',lang:'en-US',voiceURI:'zira'}];run('loadVoices()');assert.equal(run('voiceSelect.value'),'');const noMale=utterances.length;run("voiceReply('Stand by.')");assert.equal(utterances.length,noMale);assert(elements.notice.textContent.includes('No American male voice'));run('finishCaptions()');
 synthesis.getVoices=()=>[{name:'Google UK English Male',lang:'en-GB',voiceURI:'google-uk-male'},{name:'Google UK English Female',lang:'en-GB',voiceURI:'google-uk-female'},{name:'Microsoft David',lang:'en-US',voiceURI:'david-us'}];run('loadVoices()');
 run("selectAvatar('hilly')");assert.equal(run('voiceSelect.value'),'google-uk-female');run("selectAvatar('holly')");assert.equal(run('voiceSelect.value'),'google-uk-male');run("selectAvatar('queeg')");assert.equal(run('voiceSelect.value'),'david-us');
 run('searchPending=true;updateThinking()');assert.equal(elements['thinking-label'].textContent,'Queeg is thinking...');run('searchPending=false');
 responses.push({ok:true,status:200,text:'Queeg: I am thinking. Stand by for the answer.'});await run("chat('QUEEG explain a quasar')");assert.equal(run('avatar'),'queeg');assert.equal(run('searchPending'),true);
 run('finishCaptions();searchPending=false');
 responses.push({ok:true,status:200,text:'Queeg: Proceed.'});await run("chat('Queegish is not a trigger')");assert.equal(run('avatar'),'queeg');run('finishCaptions()');
 responses.push({ok:true,status:200,text:'Holly: Back.'});await run("chat('HOLLY')");assert.equal(run('avatar'),'holly');run('finishCaptions()');
 for(let i=0;i<60;i++)await Promise.resolve();
 // Retry a dropped HDMI update without disabling the stream or warning at once.
 speechFailures=1;run('hdmiBegin()');const generation=run('hdmiGeneration');run('hdmiBegin()');assert.equal(run('hdmiGeneration'),generation);
 for(let i=0;i<60;i++)await Promise.resolve();assert.equal(run('hdmiActive'),true);assert.equal(run('hdmiNeedsBegin'),true);assert.equal(run('hdmiFailures'),1);
 tick(clock+500);for(let i=0;i<60;i++)await Promise.resolve();assert.equal(run('hdmiFailures'),0);assert.equal(run('hdmiNeedsBegin'),false);run('hdmiEnd()');
 for(let i=0;i<60;i++)await Promise.resolve();assert.equal(run('hdmiActive'),false);
 run('cancelVoice();finishCaptions();busy=false;token="a".repeat(32)');pendingAlarm='7|Emergency! There is an emergency going on... it\'s still going on. Alarm: Check Starbug';
 const beforeAlarm=utterances.length;await run('assistantMaintenance(assistantEpoch)');
 assert.equal(utterances.length,beforeAlarm+1);
 for(let i=0;run('speaking')&&i<8;i++)utterances.at(-1).onend();
 assert.equal(run('speaking'),false);assert.equal(utterances.slice(beforeAlarm).map(u=>u.text).join(' '),'Emergency! There is an emergency going on... it\'s still going on. Alarm: Check Starbug');
 assert(assistantRequests.some(r=>r.path==='/alerts'&&r.opts.body==='ack 7'));pendingAlarm='';
 run('cancelVoice();finishCaptions();lastClockSync=0');await run('assistantMaintenance(assistantEpoch)');
 assert(assistantRequests.filter(r=>r.path==='/clock').length>=3);
 run('cancelVoice();finishCaptions();newsPending=true;updateThinking()');assert.equal(elements.thinking.hidden,false);assert.equal(elements['thinking-label'].textContent,'Researching world news...');await run('assistantMaintenance(assistantEpoch)');assert.equal(run('newsPending'),true);
 newsResponse='ready|Latest world headlines from BBC News. Fixture headline.';run("voiceReply('Fetching headlines, a speech session that never sends an end event.')");assert.equal(run('speaking'),true);const cancelBeforeNews=cancelled;const newsStart=utterances.length;
 await run('assistantMaintenance(assistantEpoch)');assert.equal(run('newsPending'),false);assert.equal(elements.thinking.hidden,true);assert(utterances.length>newsStart);assert(cancelled>cancelBeforeNews);
 for(let i=0;run('speaking')&&i<8;i++)utterances.at(-1).onend();assert(utterances.slice(newsStart).map(u=>u.text).join(' ').includes('Fixture headline.'));
 run('cancelVoice();finishCaptions();newsPending=true');newsResponse="error|I couldn't fetch a verified bulletin.";await run('assistantMaintenance(assistantEpoch)');assert.equal(run('newsPending'),false);
 run('cancelVoice();finishCaptions();newsPending=true;updateThinking()');newsStatus=503;await run('assistantMaintenance(assistantEpoch)');assert.equal(run('newsPending'),false);assert.equal(elements.thinking.hidden,true);assert(elements.notice.textContent.includes('News connection interrupted'));newsStatus=200;
 run('cancelVoice();finishCaptions();newsPending=false');
 responses.push({ok:true,status:200,text:"Holly: I'm searching the Junior Encyclopedia of Space. I will read the summary."});await run("chat('What is Jupiter?')");
 assert.equal(run('searchPending'),true);assert.equal(elements.thinking.hidden,false);assert.equal(elements['thinking-label'].textContent,'Searching the Junior Encyclopedia of Space...');
 await run('assistantMaintenance(assistantEpoch)');assert.equal(run('searchPending'),true);
 const beforeSearch=utterances.length;searchResponse='ready|Jupiter fixture.';
 await run('assistantMaintenance(assistantEpoch)');assert.equal(run('searchPending'),false);assert.equal(elements.thinking.hidden,true);assert(utterances.length>beforeSearch);
 for(let i=0;run('speaking')&&i<8;i++)utterances.at(-1).onend();assert(utterances.slice(beforeSearch).map(u=>u.text).join(' ').includes('Jupiter fixture'));
 run('cancelVoice();finishCaptions()');responses.push({ok:true,status:200,text:'Holly: Jupiter fixture.'});
 await run("chat('Tell me about Jupiter')");assert.equal(run('searchPending'),false);assert.equal(elements.thinking.hidden,true);
 assert(!utterances.at(-1).text.includes('DuckDuckGo'));run('cancelVoice();finishCaptions()');
 run('cancelVoice();finishCaptions();searchPending=true;updateThinking()');searchStatus=503;
 await run('assistantMaintenance(assistantEpoch)');assert.equal(run('searchPending'),false);assert.equal(elements.thinking.hidden,true);assert(elements.notice.textContent.includes('Search connection interrupted'));searchStatus=200,speechFailures=0;
 run('cancelVoice();finishCaptions();searchPending=true');searchResponse="error|I couldn't get a summary.";await run('assistantMaintenance(assistantEpoch)');assert.equal(run('searchPending'),false);
 run('cancelVoice();finishCaptions();searchPending=true');searchResponse='idle|';await run('assistantMaintenance(assistantEpoch)');assert.equal(run('searchPending'),false);
 run('cancelVoice();finishCaptions();searchPending=true');searchStatus=410;await run('assistantMaintenance(assistantEpoch)');assert.equal(run('token'),'');assert.equal(run('searchPending'),false);run('token="a".repeat(32)');searchStatus=200,speechFailures=0;
 run('newsPending=false;lock(true)');assert.equal(elements.thinking.hidden,false);assert.equal(elements['thinking-label'].textContent,'Holly is thinking...');
 tick(clock+21000);assert.equal(elements['thinking-label'].textContent,'Still waiting for Holly...');assert.equal(elements['thinking-elapsed'].textContent,'21s');
 run('lock(false)');assert.equal(elements.thinking.hidden,true);assert.equal(run('thinkingTimer'),0);
 assert(html.includes('role="progressbar"')&&!html.includes('aria-valuenow'));assert(html.includes('@media(prefers-reduced-motion:reduce)'));
 assert(cancelled>0);
 console.log('PASS: DuckDuckGo chat detection, research meter, pending/speech/readout, error/expired-session handling; visible thinking/research meter, elapsed/slow status, success/error cleanup, news polling during stuck speech, interrupted-fetch notice, face-only layout, hidden replies/settings, Hilly/Holly whole-word switching and persistence, Hillary/Hilary aliases, male/female Google UK voice priority, repeated-stop regression, chat/session, automatic final-speech sending, review/error/interim/long-speech guards, secure-origin gating, microphone cancel, offline feature detection, stale setup cancellation, British voice selection, speech boundary/end, interruption, playback watchdog, HDMI begin/frame/end stream, hands-free rearm/stop and bounded animation');
})().catch(e=>{console.error(e);process.exitCode=1;});
