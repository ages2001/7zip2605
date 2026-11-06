import andx_compat
from impacket.smbserver import SimpleSMBServer
from pathlib import Path
root=Path(__file__).parent
server=SimpleSMBServer(listenAddress="127.0.0.1",listenPort=139)
server.addShare("Z7TEST",str(root/"share"),"7-Zip private regression share",readOnly="no")
server.setSMB2Support(False)
server.setLogFile(str(root/"server.log"))
server.start()
