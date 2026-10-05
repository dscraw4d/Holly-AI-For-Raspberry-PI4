#!/usr/bin/env python3
"""Set Holly's clock from this computer via the existing OpenSSH client."""
import argparse,datetime,subprocess,time
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('host');p.add_argument('--user',default='Rimmer');args=p.parse_args()
offset=int(datetime.datetime.now().astimezone().utcoffset().total_seconds()/60)
subprocess.run(['ssh','-o','ConnectTimeout=60','-o','RekeyLimit=4M',args.user+'@'+args.host,'clock set '+str(int(time.time()))+' '+str(offset)],check=True)
