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


#ifndef ORCHID_WEBRTC_HPP
#define ORCHID_WEBRTC_HPP

#include <rtc_base/ssladapter.h>
#include <api/peerconnectioninterface.h>

#include <api/video_codecs/video_encoder_factory.h>
#include <api/video_codecs/video_decoder_factory.h>

#include <cppcoro/async_manual_reset_event.hpp>

#include "error.hpp"
#include "future.hpp"
#include "link.hpp"
#include "task.hpp"
#include "trace.hpp"

namespace orc {

class CreateObserver :
    public cppcoro::async_manual_reset_event,
    public webrtc::CreateSessionDescriptionObserver
{
  public:
    webrtc::SessionDescriptionInterface *description_;

  protected:
    void OnSuccess(webrtc::SessionDescriptionInterface *description) override {
        description_ = description;
        set();
    }
};

class SetObserver :
    public cppcoro::async_manual_reset_event,
    public webrtc::SetSessionDescriptionObserver
{
  protected:
    virtual void OnSuccess() {
        set();
    }
};

class Channel;

class Connection :
    public std::enable_shared_from_this<Connection>,
    //public cppcoro::async_manual_reset_event,
    public webrtc::PeerConnectionObserver
{
    friend class Channel;

  private:
    const rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_;

    std::set<Channel *> channels_;

    cppcoro::async_manual_reset_event gathered_;
    std::vector<std::string> gathering_;
    std::vector<std::string> candidates_;

  public:
    Connection();

    virtual ~Connection() {
_trace();
        peer_->Close();
    }

    webrtc::PeerConnectionInterface *operator->() {
        return peer_;
    }


    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState state) override {
        _trace();
    }

    void OnRenegotiationNeeded() override {
        _trace();
    }

    void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state) override;

    void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState state) override {
        switch (state) {
            case webrtc::PeerConnectionInterface::kIceGatheringNew:
                gathering_.clear();
            break;

            case webrtc::PeerConnectionInterface::kIceGatheringGathering:
            break;

            case webrtc::PeerConnectionInterface::kIceGatheringComplete:
                candidates_ = gathering_;
                gathered_.set();
            break;
        }
    }

    void OnIceCandidate(const webrtc::IceCandidateInterface *candidate) override {
        std::string sdp;
        candidate->ToString(&sdp);
        gathering_.push_back(sdp);
    }


    virtual void OnChannel(U<Channel> channel) = 0;
    void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> interface) override;


    task<void> Negotiate(webrtc::SessionDescriptionInterface *description) {
        rtc::scoped_refptr<SetObserver> observer(new rtc::RefCountedObject<SetObserver>());
        peer_->SetLocalDescription(observer, description);
        co_await *observer;
        co_await Schedule();
    }

    task<std::string> Negotiation(webrtc::SessionDescriptionInterface *description) {
        co_await Negotiate(description);
        co_await gathered_;
        co_await Schedule();
        std::string sdp;
        peer_->local_description()->ToString(&sdp);
        co_return sdp;
    }

    task<std::string> Offer() {
        co_return co_await Negotiation(co_await [&]() -> task<webrtc::SessionDescriptionInterface *> {
            rtc::scoped_refptr<CreateObserver> observer(new rtc::RefCountedObject<CreateObserver>());
            webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
            peer_->CreateOffer(observer, options);
            co_await *observer;
            co_await Schedule();
            co_return observer->description_;
        }());
    }


    task<void> Negotiate(const char *type, const std::string &sdp) {
        webrtc::SdpParseError error;
        auto answer(webrtc::CreateSessionDescription(type, sdp, &error));
        _assert(answer != NULL);
        rtc::scoped_refptr<SetObserver> observer(new rtc::RefCountedObject<SetObserver>());
        peer_->SetRemoteDescription(observer, answer);
        co_await *observer;
        co_await Schedule();
    }

    task<std::string> Answer(const std::string &offer) {
        co_await Negotiate("offer", offer);
        co_return co_await Negotiation(co_await [&]() -> task<webrtc::SessionDescriptionInterface *> {
            rtc::scoped_refptr<orc::CreateObserver> observer(new rtc::RefCountedObject<orc::CreateObserver>());
            webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
            peer_->CreateAnswer(observer, options);
            co_await *observer;
            co_await Schedule();
            co_return observer->description_;
        }());
    }

    task<void> Negotiate(const std::string &sdp) {
        co_return co_await Negotiate("answer", sdp);
    }
};

class Channel :
    public Link,
    public webrtc::DataChannelObserver,
    public cppcoro::async_manual_reset_event
{
  private:
    const S<Connection> connection_;
    const rtc::scoped_refptr<webrtc::DataChannelInterface> channel_;
    cppcoro::async_manual_reset_event closed_;

  public:
    Channel(const S<Connection> &connection, const rtc::scoped_refptr<webrtc::DataChannelInterface> &channel) :
        connection_(connection),
        channel_(channel)
    {
        channel_->RegisterObserver(this);
        connection_->channels_.insert(this);
    }

    Channel(const S<Connection> &connection, const std::string &label = std::string(), const std::string &protocol = std::string()) :
        Channel(connection, [&]() {
            webrtc::DataChannelInit init;
            init.protocol = protocol;
            return (*connection)->CreateDataChannel(label, &init);
        }())
    {
    }

    virtual ~Channel() {
        connection_->channels_.erase(this);
        channel_->UnregisterObserver();
    }

    S<Connection> Connection() {
        return connection_;
    }

    void OnStateChange() override {
        switch (channel_->state()) {
            case webrtc::DataChannelInterface::kConnecting:
_trace();
                break;
            case webrtc::DataChannelInterface::kOpen:
_trace();
                set();
                break;
            case webrtc::DataChannelInterface::kClosing:
_trace();
                break;
            case webrtc::DataChannelInterface::kClosed:
_trace();
                Close();
                break;
        }
    }

    void OnBufferedAmountChange(uint64_t previous) override {
        auto current(channel_->buffered_amount());
        std::cerr << "channel: " << current << " (" << previous << ")" << std::endl;
    }

    void OnMessage(const webrtc::DataBuffer &buffer) override {
        Beam data(reinterpret_cast<const char *>(buffer.data.data()), buffer.data.size());
        //std::cerr << "WebRTC >>> " << this << " " << data << std::endl;
        Land(data);
    }

    task<void> Send(const Buffer &data) override {
        //std::cerr << "WebRTC <<< " << this << " " << data << std::endl;
        Beam beam(data);
        channel_->Send(webrtc::DataBuffer(rtc::CopyOnWriteBuffer(beam.data(), beam.size()), true));
        co_return;
    }

    void Close() {
        closed_.set();
        Land();
    }
};

}

#endif//ORCHID_WEBRTC_HPP