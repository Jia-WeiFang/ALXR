use super::StreamId;
use crate::{Ldc, LOCAL_IP};
use alvr_common::prelude::*;
use bytes::{Buf, Bytes, BytesMut};
use futures::{
    stream::{SplitSink, SplitStream},
    StreamExt,
};
use std::{
    collections::HashMap,
    net::{IpAddr, SocketAddr},
    sync::Arc,
};
use tokio::{
    net::UdpSocket,
    sync::{mpsc, Mutex},
};
use tokio_util::udp::UdpFramed;

use std::io;
use std::error::Error;
use std::str::FromStr;
use std::fs::File;
use std::io::Write;

#[allow(clippy::type_complexity)]
#[derive(Clone)]
pub struct UdpStreamSendSocket {
    pub peer_addr: SocketAddr,
    pub inner: Arc<Mutex<SplitSink<UdpFramed<Ldc>, (Bytes, SocketAddr)>>>,
}

// peer_addr is needed to check that the packet comes from the desired device. Connecting directly
// to the peer is not supported by UdpFramed.
pub struct UdpStreamReceiveSocket {
    pub peer_addr: SocketAddr,
    pub inner: SplitStream<UdpFramed<Ldc>>,
}

pub async fn bind(port: u16) -> StrResult<UdpSocket> {
    trace_err!(UdpSocket::bind((LOCAL_IP, port)).await)
}

pub async fn connect(
    socket: UdpSocket,
    peer_ip: IpAddr,
    port: u16,
) -> StrResult<(UdpStreamSendSocket, UdpStreamReceiveSocket)> {
    let peer_addr = (peer_ip, port).into();
    let socket = UdpFramed::new(socket, Ldc::new());
    let (send_socket, receive_socket) = socket.split();

    Ok((
        UdpStreamSendSocket {
            peer_addr,
            inner: Arc::new(Mutex::new(send_socket)),
        },
        UdpStreamReceiveSocket {
            peer_addr,
            inner: receive_socket,
        },
    ))
}

// [jw] begin
pub async fn accept_from_client(
    socket: UdpSocket,
    peer_ip: IpAddr,
    port: u16,
) -> StrResult<(UdpStreamSendSocket, UdpStreamReceiveSocket)> {
    let mut buf = [0u8; 200];
    let (mut number_of_bytes, mut src_addr) = socket.recv_from(&mut buf).await.unwrap();
    info!("Source addr: {}", src_addr);
    let peer_addr = src_addr;

    let buf = &mut buf[..number_of_bytes];
    buf.reverse();
    if std::str::from_utf8(&buf).is_ok() {
        info!("[CT]: ([UDP] receive from client) {}",std::str::from_utf8(&buf).unwrap()); 
    }
    else {
        info!("[CT]: ([UDP] receive from client)  Receive {}", number_of_bytes);
    }
    trace_err!(socket.connect(src_addr).await)?;

    let hello = String::from("TNEILC OT IH DNES");
    let mut wbuf = [0u8; 200];
    let wbuf = hello.as_bytes();
    let mut buf = [0u8; 200];
    loop{
        match socket.send(&wbuf).await {
            Ok(n) => {
                info!("[CT] server send UDP success");
            }
            Err(e) => {
                info!("[CT] server send UDP failed");
            }
        }    
        match socket.recv(&mut buf).await {
            Ok(n) => {
                buf.reverse();
                if std::str::from_utf8(&buf).is_ok() {
                    info!("[CT]: ([UDP] receive from client) {}",std::str::from_utf8(&buf).unwrap()); 
                    break;
                }
                else {
                    info!("[CT]: ([UDP] receive from client) Receive {}", n);
                }
            }
            Err(e) => {
                info!("[CT]: Server receive UDP failed");
            }
                
        }
        
    }
    
    let socket = UdpFramed::new(socket, Ldc::new());
    let (send_socket, receive_socket) = socket.split();

    Ok((
        UdpStreamSendSocket {
            peer_addr,
            inner: Arc::new(Mutex::new(send_socket)),
        },
        UdpStreamReceiveSocket {
            peer_addr,
            inner: receive_socket,
        },
    ))
}

pub async fn connect_to_server(
    socket: UdpSocket,
    peer_ip: IpAddr,
    port: u16,
) -> StrResult<(UdpStreamSendSocket, UdpStreamReceiveSocket)> {
    let peer_addr = (peer_ip, port).into();
    trace_err!(socket.connect(peer_addr).await)?;
    
    let mut hello = String::from("REVRES OT IH DNES");
    let mut wbuf = [0u8; 200];
    let wbuf = hello.as_bytes();
    let mut buf = [0u8; 200];
    loop{
        match socket.send(&wbuf).await {
            Ok(n) => {
                info!("[CT] client send UDP success");
            }
            Err(e) => {
                info!("[CT] client send UDP failed");
            }
        }  
        match socket.recv(&mut buf).await {
            Ok(n) => {
                if std::str::from_utf8(&buf).is_ok() {
                    info!("[CT]: ([UDP] receive from server) {}",std::str::from_utf8(&buf).unwrap()); 
                    break;
                }
                else {
                    info!("[CT]: ([UDP] receive from server) Receive {}", n);
                }
            }
            Err(e) => {
                info!("[CT]: Client receive UDP failed");
            }
        }
    }
    
    match socket.send(&buf).await {
        Ok(n) => {
            info!("[CT] client send UDP success");
        }
        Err(e) => {
            info!("[CT] client send UDP failed");
        }
    }

    let socket = UdpFramed::new(socket, Ldc::new());
    let (send_socket, receive_socket) = socket.split();

    Ok((
        UdpStreamSendSocket {
            peer_addr,
            inner: Arc::new(Mutex::new(send_socket)),
        },
        UdpStreamReceiveSocket {
            peer_addr,
            inner: receive_socket,
        },
    ))
}
// [jw] end

pub async fn receive_loop(
    mut socket: UdpStreamReceiveSocket,
    packet_enqueuers: Arc<Mutex<HashMap<StreamId, mpsc::UnboundedSender<BytesMut>>>>,
) -> StrResult {
    while let Some(maybe_packet) = socket.inner.next().await {
        let (mut packet_bytes, address) = trace_err!(maybe_packet)?;

        if address != socket.peer_addr {
            continue;
        }

        let stream_id = packet_bytes.get_u16();
        if let Some(enqueuer) = packet_enqueuers.lock().await.get_mut(&stream_id) {
            trace_err!(enqueuer.send(packet_bytes))?;
        }
    }

    Ok(())
}
