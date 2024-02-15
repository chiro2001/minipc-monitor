#![allow(dead_code)]

use std::io::Read;

const BLE_FRAME_HEADER_SZ: usize = 2 + 1 + 1 + 1 + 4 + 1;
const BLE_FRAME_PAYLOAD_SZ: usize = 251 - BLE_FRAME_HEADER_SZ;

#[repr(C)]
pub struct BleFrame {
    pub magic: u16,
    pub payload_len: u8,
    pub channel: u8,
    pub channel_offset: u32,
    pub flags: u8,
    pub payload: [u8; BLE_FRAME_PAYLOAD_SZ],
}

impl Default for BleFrame {
    fn default() -> Self {
        BleFrame {
            magic: BLE_FRAME_MAGIC,
            payload_len: 0,
            channel: 0,
            channel_offset: 0,
            flags: 0,
            payload: [0; BLE_FRAME_PAYLOAD_SZ],
        }
    }
}

const BLE_FRAME_MAGIC: u16 = 0xbeef;

#[repr(u8)]
#[derive(Debug, Copy, Clone)]
pub enum BleFrameFlag {
    MF = 1 << 0,
}

const RPC_FUNC_LEN: usize = 48;

#[repr(C)]
pub struct RpcRequest {
    pub function: [u8; RPC_FUNC_LEN],
    pub args: Vec<u8>,
}

#[repr(C)]
pub struct RpcRequestArg {
    pub type_: u8,
    pub length: u8,
    pub data: Vec<u8>,
}

#[derive(Debug, Copy, Clone)]
pub enum RpcReqType {
    End = 0,
    Int = 1,
    Channel = 2,
}

impl Default for RpcRequest {
    fn default() -> Self {
        let mut function = [0; RPC_FUNC_LEN];
        const RPC_FUNC_DEF: &str = "invalid";
        function[..RPC_FUNC_DEF.len()].copy_from_slice(RPC_FUNC_DEF.as_bytes());
        RpcRequest {
            function,
            args: Vec::new()
        }
    }
}

impl RpcRequest {
    pub fn new(func: &str, args: &[RpcRequestArg]) -> Self {
        let mut rpc = RpcRequest::default();
        rpc.function.fill(0);
        rpc.function[..func.len()].copy_from_slice(func.as_bytes());
        rpc.args = args.iter().flat_map(|arg| {
            let mut v = vec![arg.type_];
            v.push(arg.length);
            v.extend(arg.data.iter().take(arg.length as usize));
            v
        }).collect();
        rpc
    }
}

impl BleFrame {
    pub fn new(channel: u8, channel_offset: u32, flags: u8, req: RpcRequest) -> Self {
        let mut frame = BleFrame::default();
        frame.channel = channel;
        frame.channel_offset = channel_offset;
        frame.flags = flags;
        frame.payload_len = (req.function.len() + req.args.len()) as u8;
        frame.payload[..req.function.len()].copy_from_slice(&req.function);
        frame
    }
    pub fn new_command(func: &str, args: &[RpcRequestArg]) -> Self {
        let req = RpcRequest::new(func, args);
        BleFrame::new(0, 0, 0, req)
    }
}

impl Into<Vec<u8>> for BleFrame {
    fn into(self) -> Vec<u8> {
        let mut v = Vec::new();
        v.extend_from_slice(&self.magic.to_le_bytes());
        v.push(self.payload_len);
        v.push(self.channel);
        v.extend_from_slice(&self.channel_offset.to_le_bytes());
        v.push(self.flags);
        v.extend_from_slice(self.payload.as_slice().take(self.payload_len as u64).into_inner());
        v
    }
}
