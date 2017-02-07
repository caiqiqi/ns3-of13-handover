#ifndef WIFI_CONTROLLER_H
#define WIFI_CONTROLLER_H

#include <ns3/network-module.h>
#include <ns3/internet-module.h>
#include <ns3/ofswitch13-module.h>

using namespace ns3;

/**
 * \brief An border OpenFlow 1.3 controller
 */
class WifiController : public OFSwitch13Controller
{
public:
  WifiController ();          //!< Default constructor
  virtual ~WifiController (); //!< Dummy destructor.

  /** Destructor implementation */
  virtual void DoDispose ();

  /**
   * Register this type.
   * \return The object TypeId.
   */
  static TypeId GetTypeId (void);

  // Inherited from OFSwitch13Controller
  ofl_err HandlePacketIn (ofl_msg_packet_in *msg, SwitchInfo swtch, uint32_t xid);

protected:
  // Inherited from OFSwitch13Controller
  void ConnectionStarted (SwitchInfo swtch);

private:

  /**
   * Handle ARP request messages.
   * \param msg The packet-in message.
   * \param swtch The switch information.
   * \param xid Transaction id.
   * \return 0 if everything's ok, otherwise an error number.
   */
  ofl_err HandleArpPacketIn (ofl_msg_packet_in *msg, SwitchInfo swtch, uint32_t xid);

  /**
   * Handle TCP connection request
   * --- 处理来自switch发起的连接请求
   * \param msg The packet-in message.
   * \param swtch The switch information.
   * \param xid Transaction id.
   * \return 0 if everything's ok, otherwise an error number.
   */
  ofl_err HandleConnectionRequest (ofl_msg_packet_in *msg, SwitchInfo swtch, uint32_t xid);

  /**
   * Extract an IPv4 address from packet match.
   * --- 从匹配到的数据包中得到其 IP
   * \param oxm_of The OXM_IF_* IPv4 field.
   * \param match The ofl_match structure pointer.
   * \return The IPv4 address.
   */
  Ipv4Address ExtractIpv4Address (uint32_t oxm_of, ofl_match* match);

  /**
   * Create a Packet with an ARP reply, encapsulated inside of an Ethernet frame.
   * \param srcMac Source MAC address.
   * \param srcIP Source IP address.
   * \param dstMac Destination MAC address.
   * \param dstMac Destination IP address.
   * \return The ns3 Ptr<Packet> with the ARP reply.
   */
  Ptr<Packet> CreateArpReply (Mac48Address srcMac, Ipv4Address srcIp,
                              Mac48Address dstMac, Ipv4Address dstIp);

  Address   m_serverIpAddress;    //!< Virtual server IP address
  uint16_t  m_serverTcpPort;      //!< Virtual server TCP port
  Address   m_serverMacAddress;   //!< Border switch MAC address
  bool      m_meterEnable;        //!< Enable per-flow mettering
  DataRate  m_meterRate;          //!< Per-flow meter rate
  bool      m_linkAggregation;    //!< Enable link aggregation
};

#endif /* WIFI_CONTROLLER_H */
