/* Orchid - WebRTC P2P VPN Market (on Ethereum)
 * Copyright (C) 2017-2019  The Orchid Authors
*/

/* GNU Affero General Public License, Version 3 {{{ */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */


#ifndef ORCHID_REMOTE_HPP
#define ORCHID_REMOTE_HPP

#include <lwip/netif.h>

#include "nest.hpp"
#include "origin.hpp"
#include "socket.hpp"

namespace orc {

class Remote :
    public Origin,
    public BufferDrain
{
  private:
    const class Host host_;
    Nest nest_;

    netif interface_;

    void Send(pbuf *buffer);
    static err_t Output(netif *interface, pbuf *buffer, const ip4_addr_t *destination);
    static err_t Initialize(netif *interface);

  protected:
    virtual Pump<Buffer> *Inner() noexcept = 0;

    void Land(const Buffer &data) override;
    void Stop(const std::string &error) noexcept override;

  private:
    Remote(const class Host &host);
  public:
    Remote();
    ~Remote() override;

    void Open();
    task<void> Shut() noexcept override;

    class Host Host() override;

    rtc::Thread *Thread() override;
    rtc::BasicPacketSocketFactory &Factory() override;

    task<void> Associate(Sunk<> *sunk, const Socket &endpoint) override;
    task<Socket> Unlid(Sunk<BufferSewer, Opening> *sunk) override;
    task<void> Connect(U<Stream> &stream, const Socket &endpoint) override;
};

}

#endif//ORCHID_REMOTE_HPP
