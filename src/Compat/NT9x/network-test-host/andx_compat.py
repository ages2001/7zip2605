"""SMB1 AndX request chaining for the private legacy-client test server."""
import struct
from impacket import smb, smbserver
_original = smbserver.SMBSERVER.processRequest
def _run(server, conn_id, data):
    try:
        result = _original(server, conn_id, data)
    except FileNotFoundError:
        # SMB_QUERY_INFORMATION must return a missing-file status, not close
        # the transport when clients probe an output path before creating it.
        if data[:4] != b'\xffSMB' or data[4] != 0x08:
            raise
        request = smb.NewSMBPacket(data=data)
        packet = smb.NewSMBPacket()
        packet['Flags1'] = smb.SMB.FLAGS1_REPLY
        packet['Flags2'] = smb.SMB.FLAGS2_NT_STATUS
        for field in ['Tid', 'Uid', 'Mid', 'Pid']:
            packet[field] = request[field]
        packet['ErrorClass'], packet['_reserved'], packet['ErrorCode'] = 0x34, 0, 0xc000
        command = smb.SMBCommand(request['Command'])
        command['Parameters'], command['Data'] = b'', b''
        packet.addCommand(command)
        return [packet]
    for packet in result:
        if data[:4] == b'\xffSMB':
            if not (struct.unpack_from('<H', data, 10)[0] & smb.SMB.FLAGS2_EXTENDED_SECURITY):
                packet['Flags2'] &= ~smb.SMB.FLAGS2_EXTENDED_SECURITY
            for command in packet['Data']:
                if command.command == 0x75 and command['Data']['Service'] != 'IPC':
                    command['Data']['Service'] = 'A:'
    return result
_ANDX = {0x24, 0x2d, 0x2e, 0x2f, 0x73, 0x74, 0x75, 0xa2}
def process_request(self, conn_id, data):
    if data[:4] != b'\xffSMB' or data[4] not in _ANDX:
        return _run(self, conn_id, data)
    request = smb.NewSMBPacket(data=data)
    cmd = smb.SMBCommand(request['Data'][0])
    if len(cmd['Parameters']) < 4 or cmd['Parameters'][0] == 255:
        return _run(self, conn_id, data)
    command, offset, seen = request['Command'], 32, set()
    combined = None
    while command != 255:
        if offset in seen or offset < 32 or offset >= len(data):
            raise ValueError('Invalid AndX chain offset')
        seen.add(offset)
        part = smb.NewSMBPacket(data=data[:32]+data[offset:])
        part['Command'] = command
        if combined is not None:
            part['Uid'], part['Tid'] = combined['Uid'], combined['Tid']
        params = smb.SMBCommand(part['Data'][0])['Parameters']
        next_command, next_offset = (255, 0)
        if command in _ANDX and len(params) >= 4:
            next_command, next_offset = params[0], struct.unpack_from('<H', params, 2)[0]
        responses = _run(self, conn_id, part.getData())
        if len(responses) != 1:
            raise ValueError('Unsupported multipart response in AndX chain')
        response = responses[0]
        if combined is None:
            combined = response
        else:
            combined['Uid'], combined['Tid'] = response['Uid'], response['Tid']
            for field in ['ErrorClass', '_reserved', 'ErrorCode']:
                combined[field] = response.fields.get(field, 0)
            for result in response['Data']:
                combined.addCommand(result)
        if response.fields.get('ErrorClass', 0) or response.fields.get('ErrorCode', 0):
            break
        command, offset = next_command, next_offset
    return [combined]
smbserver.SMBSERVER.processRequest = process_request
