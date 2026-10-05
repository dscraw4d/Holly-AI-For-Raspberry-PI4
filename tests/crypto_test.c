#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "sha256.h"
static void check(const uint8_t *digest,const char *hex){
    static const char chars[]="0123456789abcdef";
    for(unsigned i=0;i<32;i++){
        assert(hex[i*2]==chars[digest[i]>>4]);
        assert(hex[i*2+1]==chars[digest[i]&15]);
    }
}
int main(void){
    uint8_t digest[32],other[32];struct holly_sha256 c;
    assert(holly_sha256_hash(0,0,digest)==0);
    check(digest,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    assert(holly_sha256_hash("abc",3,digest)==0);
    check(digest,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const char *long_message="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    holly_sha256_init(&c);
    for(size_t i=0;i<strlen(long_message);i++)assert(holly_sha256_update(&c,long_message+i,1)==0);
    assert(holly_sha256_finish(&c,digest)==0);
    check(digest,"248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    assert(holly_sha256_update(&c,"a",1)==-1);
    assert(holly_sha256_finish(&c,digest)==-1);
    holly_sha256_init(&c);
    uint8_t thousand[1000];memset(thousand,'a',sizeof(thousand));
    for(unsigned i=0;i<1000;i++)assert(holly_sha256_update(&c,thousand,1000)==0);
    assert(holly_sha256_finish(&c,digest)==0);
    check(digest,"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    holly_sha256_init(&c);c.bytes=UINT64_MAX/8;
    assert(holly_sha256_update(&c,"a",1)==-1 && c.bytes==UINT64_MAX/8);
    assert(holly_sha256_hash(0,1,digest)==-1);
    assert(holly_sha256_hash("a",1,0)==-1);
    uint8_t key[131];memset(key,0x0b,20);
    assert(holly_hmac_sha256(key,20,"Hi There",8,digest)==0);
    check(digest,"b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
    assert(holly_hmac_sha256("Jefe",4,"what do ya want for nothing?",28,digest)==0);
    check(digest,"5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    memset(key,0xaa,sizeof(key));
    const char *data="Test Using Larger Than Block-Size Key - Hash Key First";
    assert(holly_hmac_sha256(key,sizeof(key),data,strlen(data),digest)==0);
    check(digest,"60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
    assert(holly_hmac_sha256(0,1,data,strlen(data),digest)==-1);
    memcpy(other,digest,32);assert(holly_tag_equal(other,digest,32)==1);
    for(unsigned i=0;i<32;i++){
        other[i]^=1;assert(holly_tag_equal(other,digest,32)==0);other[i]^=1;
    }
    assert(holly_tag_equal(0,digest,32)==0);
    puts("SHA-256 known answers, streaming, lifecycle and RFC 4231 HMAC tests passed");
}
