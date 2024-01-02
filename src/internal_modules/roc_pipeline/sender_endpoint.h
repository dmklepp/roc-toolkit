/*
 * Copyright (c) 2017 Roc Streaming authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//! @file roc_pipeline/sender_endpoint.h
//! @brief Sender endpoint pipeline.

#ifndef ROC_PIPELINE_SENDER_ENDPOINT_H_
#define ROC_PIPELINE_SENDER_ENDPOINT_H_

#include "roc_address/protocol.h"
#include "roc_core/iarena.h"
#include "roc_core/mpsc_queue.h"
#include "roc_core/noncopyable.h"
#include "roc_core/optional.h"
#include "roc_core/scoped_ptr.h"
#include "roc_packet/icomposer.h"
#include "roc_packet/iparser.h"
#include "roc_packet/iwriter.h"
#include "roc_packet/shipper.h"
#include "roc_pipeline/state_tracker.h"
#include "roc_rtcp/composer.h"
#include "roc_rtcp/parser.h"
#include "roc_rtp/composer.h"

namespace roc {
namespace pipeline {

class SenderSession;

//! Sender endpoint sub-pipeline.
//!
//! Contains:
//!  - a pipeline for processing packets for single network endpoint
class SenderEndpoint : public core::NonCopyable<>, private packet::IWriter {
public:
    //! Initialize.
    //!  - @p outbound_address specifies destination address that is assigned to the
    //!    outgoing packets in the end of endpoint pipeline
    //!  - @p outbound_writer specifies destination writer to which packets are sent
    //!    in the end of endpoint pipeline
    SenderEndpoint(address::Protocol proto,
                   StateTracker& state_tracker,
                   SenderSession& sender_session,
                   const address::SocketAddr& outbound_address,
                   packet::IWriter& outbound_writer,
                   core::IArena& arena);

    //! Check if pipeline was succefully constructed.
    bool is_valid() const;

    //! Get protocol.
    address::Protocol proto() const;

    //! Get packet composer.
    //! @remarks
    //!  This composer will creates packets according to endpoint protocol.
    packet::IComposer& composer();

    //! Get writer for outbound packets.
    //! This way packets generated by sender reach network.
    //! @remarks
    //!  Packets passed to this writer will be enqueued for sending.
    //!  When frame is written to SenderSession, it generates packets
    //!  and writes them to outbound writers of endpoints.
    packet::IWriter& outbound_writer();

    //! Get writer for inbound packets.
    //! This way feedback packets from receiver reach sender pipeline.
    //! @remarks
    //!  Packets passed to this writer will be pulled into pipeline.
    //!  This writer is thread-safe and lock-free, packets can be written
    //!  to it from netio thread.
    //!  pull_packets() will pull enqueued inbound packets into SenderSession,
    //!  which will use them next time when frame is written.
    //! @note
    //!  Not all protocols support inbound packets on sender. If it's
    //!  not supported, the method returns NULL.
    packet::IWriter* inbound_writer();

    //! Pull packets written to inbound writer into pipeline.
    //! @remarks
    //!  Packets are written to inbound_writer() from network thread.
    //!  They don't appear in pipeline immediately. Instead, pipeline thread
    //!  should periodically call pull_packets() to make them available.
    ROC_ATTR_NODISCARD status::StatusCode pull_packets(core::nanoseconds_t current_time);

private:
    virtual ROC_ATTR_NODISCARD status::StatusCode write(const packet::PacketPtr& packet);

    const address::Protocol proto_;

    StateTracker& state_tracker_;
    SenderSession& sender_session_;

    // Outbound packets sub-pipeline.
    // On sender, always present.
    packet::IComposer* composer_;
    core::Optional<rtp::Composer> rtp_composer_;
    core::ScopedPtr<packet::IComposer> fec_composer_;
    core::Optional<rtcp::Composer> rtcp_composer_;
    core::Optional<packet::Shipper> shipper_;

    // Inbound packets sub-pipeline.
    // On sender, typically present only in control endpoints.
    packet::IParser* parser_;
    core::Optional<rtcp::Parser> rtcp_parser_;
    core::MpscQueue<packet::Packet> inbound_queue_;

    bool valid_;
};

} // namespace pipeline
} // namespace roc

#endif // ROC_PIPELINE_SENDER_ENDPOINT_H_
