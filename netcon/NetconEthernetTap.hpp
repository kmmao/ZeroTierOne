/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef ZT_NETCONETHERNETTAP_HPP
#define ZT_NETCONETHERNETTAP_HPP

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>
#include <stdexcept>

#include "../node/Constants.hpp"
#include "../node/MulticastGroup.hpp"
#include "../node/Mutex.hpp"
#include "../node/InetAddress.hpp"
#include "../osdep/Thread.hpp"
#include "../osdep/Phy.hpp"

#include "netif/etharp.h"

struct tcp_pcb;
struct socket_st;
struct listen_st;
struct bind_st;
struct connect_st;

namespace ZeroTier {

class NetconEthernetTap;
class TcpConnection;
class Larg;
class LWIPStack;

/**
 * Network Containers instance -- emulates an Ethernet tap device as far as OneService knows
 */
class NetconEthernetTap
{
	friend class Phy<NetconEthernetTap *>;

public:
	NetconEthernetTap(
		const char *homePath,
		const MAC &mac,
		unsigned int mtu,
		unsigned int metric,
		uint64_t nwid,
		const char *friendlyName,
		void (*handler)(void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int),
		void *arg);

	~NetconEthernetTap();

	void setEnabled(bool en);
	bool enabled() const;
	bool addIp(const InetAddress &ip);
	bool removeIp(const InetAddress &ip);
	std::vector<InetAddress> ips() const;
	void put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len);
	std::string deviceName() const;
	void setFriendlyName(const char *friendlyName);
	void scanMulticastGroups(std::vector<MulticastGroup> &added,std::vector<MulticastGroup> &removed);

	void threadMain()
		throw();

	LWIPStack *lwipstack;
  uint64_t _nwid;
  void (*_handler)(void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int);
  void *_arg;

private:
	// LWIP callbacks
	static err_t nc_poll(void* arg, struct tcp_pcb *tpcb);
	static err_t nc_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
	static err_t nc_recved(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
	static void nc_err(void *arg, err_t err);
	static void nc_close(struct tcp_pcb *tpcb);
	static err_t nc_send(struct tcp_pcb *tpcb);
	static err_t nc_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
	static err_t nc_connected(void *arg, struct tcp_pcb *tpcb, err_t err);

	// RPC handlers (from NetconIntercept)
	void unload_rpc(void *data, pid_t &pid, pid_t &tid, int &rpc_count, char (timestamp[20]), char &cmd, void* &payload);

	void handle_bind(PhySocket *sock, void **uptr, struct bind_st *bind_rpc);
	void handle_listen(PhySocket *sock, void **uptr, struct listen_st *listen_rpc);
	void handle_map_request(PhySocket *sock, void **uptr, unsigned char* buf);
	void handle_retval(PhySocket *sock, void **uptr, int rpc_count, int newfd);
	TcpConnection * handle_socket(PhySocket *sock, void **uptr, struct socket_st* socket_rpc);
	void handle_connect(PhySocket *sock, void **uptr, struct connect_st* connect_rpc);
	void handle_write(TcpConnection *conn);

	int send_return_value(TcpConnection *conn, int retval, int _errno);
	int send_return_value(int fd, int retval, int _errno);

	void phyOnDatagram(PhySocket *sock,void **uptr,const struct sockaddr *from,void *data,unsigned long len);
	void phyOnTcpConnect(PhySocket *sock,void **uptr,bool success);
	void phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from);
	void phyOnTcpClose(PhySocket *sock,void **uptr);
	void phyOnTcpData(PhySocket *sock,void **uptr,void *data,unsigned long len);
	void phyOnTcpWritable(PhySocket *sock,void **uptr);
	void phyOnUnixAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN);
	void phyOnUnixClose(PhySocket *sock,void **uptr);
	void phyOnUnixData(PhySocket *sock,void **uptr,void *data,unsigned long len);
	void phyOnFileDescriptorActivity(PhySocket *sock,void **uptr,bool readable,bool writable);

	ip_addr_t convert_ip(struct sockaddr_in * addr)
	{
	  ip_addr_t conn_addr;
	  struct sockaddr_in *ipv4 = addr;
	  short a = ip4_addr1(&(ipv4->sin_addr));
	  short b = ip4_addr2(&(ipv4->sin_addr));
	  short c = ip4_addr3(&(ipv4->sin_addr));
	  short d = ip4_addr4(&(ipv4->sin_addr));
	  IP4_ADDR(&conn_addr, a,b,c,d);
	  return conn_addr;
	}

	// Client helpers
	TcpConnection *getConnectionByTheirFD(PhySocket *sock, int fd);
	void closeConnection(TcpConnection *conn);
	void closeAll();
	void closeClient(PhySocket *sock);
	void compact_dump();
	void dump();
	void die(int exret);

	Phy<NetconEthernetTap *> _phy;
	PhySocket *_unixListenSocket;

	std::vector<TcpConnection*> tcp_connections;
	std::vector<PhySocket*> rpc_sockets;
	std::map<PhySocket*, pid_t> pidmap;
	pid_t rpc_counter;

	netif interface;

	MAC _mac;
	Thread _thread;
	std::string _homePath;
	std::string _dev; // path to Unix domain socket

	std::vector<MulticastGroup> _multicastGroups;
	Mutex _multicastGroups_m;

	std::vector<InetAddress> _ips;
	Mutex _ips_m;

	unsigned int _mtu;
	volatile bool _enabled;
	volatile bool _run;
};

} // namespace ZeroTier

#endif
