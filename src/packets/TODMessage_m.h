//
// Generated file, do not edit! Created by opp_msgtool 6.0 from packets/TODMessage.msg.
//

#ifndef __INET_TODMESSAGE_M_H
#define __INET_TODMESSAGE_M_H

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif
#include <omnetpp.h>

// opp_msgtool version check
#define MSGC_VERSION 0x0600
#if (MSGC_VERSION!=OMNETPP_VERSION)
#    error Version mismatch! Probably this file was generated by an earlier version of opp_msgtool: 'make clean' should help.
#endif


namespace inet {

class TODMessage;

}  // namespace inet

#include "inet/common/INETDefs_m.h" // import inet.common.INETDefs

#include "inet/common/Units_m.h" // import inet.common.Units

#include "inet/common/packet/chunk/Chunk_m.h" // import inet.common.packet.chunk.Chunk

// cplusplus {{
#include "inet/common/packet/ChunkBuffer.h"
#include "inet/common/packet/ReassemblyBuffer.h"
#include "inet/common/packet/ReorderBuffer.h"
// }}


namespace inet {

/**
 * Class generated from <tt>packets/TODMessage.msg:27</tt> by opp_msgtool.
 * <pre>
 * class TODMessage extends FieldsChunk
 * {
 *     simtime_t creationTime;
 *     int msgId;
 * }
 * </pre>
 */
class TODMessage : public ::inet::FieldsChunk
{
  protected:
    ::omnetpp::simtime_t creationTime = SIMTIME_ZERO;
    int msgId = 0;

  private:
    void copy(const TODMessage& other);

  protected:
    bool operator==(const TODMessage&) = delete;

  public:
    TODMessage();
    TODMessage(const TODMessage& other);
    virtual ~TODMessage();
    TODMessage& operator=(const TODMessage& other);
    virtual TODMessage *dup() const override {return new TODMessage(*this);}
    virtual void parsimPack(omnetpp::cCommBuffer *b) const override;
    virtual void parsimUnpack(omnetpp::cCommBuffer *b) override;

    virtual ::omnetpp::simtime_t getCreationTime() const;
    virtual void setCreationTime(::omnetpp::simtime_t creationTime);

    virtual int getMsgId() const;
    virtual void setMsgId(int msgId);
};

inline void doParsimPacking(omnetpp::cCommBuffer *b, const TODMessage& obj) {obj.parsimPack(b);}
inline void doParsimUnpacking(omnetpp::cCommBuffer *b, TODMessage& obj) {obj.parsimUnpack(b);}


}  // namespace inet


namespace omnetpp {

template<> inline inet::TODMessage *fromAnyPtr(any_ptr ptr) { return check_and_cast<inet::TODMessage*>(ptr.get<cObject>()); }

}  // namespace omnetpp

#endif // ifndef __INET_TODMESSAGE_M_H

