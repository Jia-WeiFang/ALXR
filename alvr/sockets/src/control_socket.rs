use super::{Ldc, CONTROL_PORT, LOCAL_IP};
use alvr_common::prelude::*;
use bytes::Bytes;
use futures::{
    stream::{SplitSink, SplitStream},
    SinkExt, StreamExt,
};
use serde::{de::DeserializeOwned, Serialize};
use std::{marker::PhantomData, net::IpAddr};
use tokio::net::{TcpListener, TcpStream};
use tokio_util::codec::Framed;

use std::io;
use std::error::Error;

pub struct ControlSocketSender<T> {
    inner: SplitSink<Framed<TcpStream, Ldc>, Bytes>,
    _phantom: PhantomData<T>,
}

impl<S: Serialize> ControlSocketSender<S> {
    pub async fn send(&mut self, packet: &S) -> StrResult {
        let packet_bytes = trace_err!(bincode::serialize(packet))?;
        trace_err!(self.inner.send(packet_bytes.into()).await)
    }
}

pub struct ControlSocketReceiver<T> {
    inner: SplitStream<Framed<TcpStream, Ldc>>,
    _phantom: PhantomData<T>,
}

impl<R: DeserializeOwned> ControlSocketReceiver<R> {
    pub async fn recv(&mut self) -> StrResult<R> {
        let packet_bytes = trace_err!(trace_none!(self.inner.next().await)?)?;
        trace_err!(bincode::deserialize(&packet_bytes))
    }
}

// Proto-control-socket that can send and receive any packet. After the split, only the packets of
// the specified types can be exchanged
pub struct ProtoControlSocket {
    inner: Framed<TcpStream, Ldc>,
}

// pub enum PeerType {
//     AnyClient(Vec<IpAddr>),
//     Server,
// }

// [CT] begin
pub enum PeerType {
    AnyClient,
    Server,
}
// [CT] end

impl ProtoControlSocket {
    // pub async fn connect_to(peer: PeerType) -> StrResult<(Self, IpAddr)> {
    //     let socket = match peer {
    //         PeerType::AnyClient(ips) => {
    //             let client_addresses = ips
    //                 .iter()
    //                 .map(|&ip| (ip, CONTROL_PORT).into())
    //                 .collect::<Vec<_>>();
    //             trace_err!(TcpStream::connect(client_addresses.as_slice()).await)?
    //         }
    //         PeerType::Server => {
    //             let listener = trace_err!(TcpListener::bind((LOCAL_IP, CONTROL_PORT)).await)?;
    //             let (socket, _) = trace_err!(listener.accept().await)?;
    //             socket
    //         }
    //     };

    //     trace_err!(socket.set_nodelay(true))?;
    //     let peer_ip = trace_err!(socket.peer_addr())?.ip();
    //     let socket = Framed::new(socket, Ldc::new());

    //     Ok((Self { inner: socket }, peer_ip))
    // }

    // [CT] begin
    pub async fn connect_to(peer: PeerType) -> StrResult<(Self, IpAddr)> {
        let socket = match peer {
            PeerType::AnyClient => {
                let listener = trace_err!(TcpListener::bind((LOCAL_IP, CONTROL_PORT)).await)?;
                let (socket, _) = trace_err!(listener.accept().await)?;
                let mut buf = [0u8; 1500];
                let hello = String::from("LSMN YPPAH [tekcos lortnoc]");
                let mut wbuf = [0u8; 1500];
                let wbuf = hello.as_bytes();
                loop{
                    socket.readable().await;
                    match socket.try_read(&mut buf) {
                        Ok(0) => {},
                        Ok(n) => {
                            buf.reverse();
                            if std::str::from_utf8(&buf).is_ok() {
                                info!("[CT]: (control socket) {}",std::str::from_utf8(&buf).unwrap()); 
                                
                            }
                            else {
                                info!("[CT]:  Receive {}", n);
                            }
                        }
                        Err(ref e) if e.kind() == io::ErrorKind::WouldBlock => {}
                        Err(e) => {}
                    }
                    socket.writable().await;
                    match socket.try_write(&wbuf) {
                        Ok(n) => {
                            break;
                        }
                        Err(ref e) if e.kind() == io::ErrorKind::WouldBlock => {}
                        Err(e) => {}
                    }
                }
                socket
                
            }
            PeerType::Server => {
                let socket = trace_err!(TcpStream::connect("140.114.79.75:9943").await)?;
                let hello = String::from("LSMN YPPAH [tekcos lortnoc]");
                let mut wbuf = [0u8; 1500];
                let wbuf = hello.as_bytes();
                let mut buf = [0u8; 1500];
                loop{
                    socket.writable().await;
                    match socket.try_write(&wbuf) {
                        Ok(n) => {
                            
                        }
                        Err(ref e) if e.kind() == io::ErrorKind::WouldBlock => {}
                        Err(e) => {}
                    }
                    socket.readable().await;
                    match socket.try_read(&mut buf) {
                        Ok(0) => {},
                        Ok(n) => {
                            buf.reverse();
                            if std::str::from_utf8(&buf).is_ok() {
                                break;
                                
                            }
                            else {
                                // info!("[CT]:  Receive {}", n);
                            }
                        }
                        Err(ref e) if e.kind() == io::ErrorKind::WouldBlock => {}
                        Err(e) => {}
                    }
                }
                socket
            }
        };
        // [CT] end

        trace_err!(socket.set_nodelay(true))?;
        let peer_ip = trace_err!(socket.peer_addr())?.ip();
        let socket = Framed::new(socket, Ldc::new());

        Ok((Self { inner: socket }, peer_ip))
    }

    pub async fn send<S: Serialize>(&mut self, packet: &S) -> StrResult {
        let packet_bytes = trace_err!(bincode::serialize(packet))?;
        trace_err!(self.inner.send(packet_bytes.into()).await)
    }

    pub async fn recv<R: DeserializeOwned>(&mut self) -> StrResult<R> {
        let packet_bytes = trace_err!(trace_none!(self.inner.next().await)?)?;
        trace_err!(bincode::deserialize(&packet_bytes))
    }

    pub fn split<S: Serialize, R: DeserializeOwned>(
        self,
    ) -> (ControlSocketSender<S>, ControlSocketReceiver<R>) {
        let (sender, receiver) = self.inner.split();

        (
            ControlSocketSender {
                inner: sender,
                _phantom: PhantomData,
            },
            ControlSocketReceiver {
                inner: receiver,
                _phantom: PhantomData,
            },
        )
    }
}
