/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Author: Mattia Bisacchi <mattia.bisacchi@studio.unibo.it> 
 */

// #ifdef NS3_OFSWITCH13

#include "ofswitch13-topology-controller.h"

#include "topology-packet.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/ethernet-header.h"
#include "ns3/ethernet-trailer.h"
#include "ns3/llc-snap-header.h"
#include "ns3/udp-header.h"

#include "topology-types.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("OFSwitch13TopologyController");
NS_OBJECT_ENSURE_REGISTERED (OFSwitch13TopologyController);

/**
 * ---> Public methods <---
 */
OFSwitch13TopologyController::OFSwitch13TopologyController ()
{
    NS_LOG_FUNCTION (this);
}

OFSwitch13TopologyController::~OFSwitch13TopologyController ()
{
    NS_LOG_FUNCTION (this);
}

TypeId
OFSwitch13TopologyController::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::OFSwitch13TopologyController")
                        .SetParent<OFSwitch13Controller> ()
                        .SetGroupName ("OFSwitch13")
                        .AddConstructor<OFSwitch13TopologyController> ()
    ;
    return tid;
}

void
OFSwitch13TopologyController::DoDispose ()
{
    // PrintNodesInformation ();

    m_learnedInfo.clear ();
    OFSwitch13Controller::DoDispose ();
}

ofl_err
OFSwitch13TopologyController::HandlePacketIn (ofl_msg_packet_in *msg,
                                              SwitchInfo swtch, uint32_t xid)
{
    NS_LOG_FUNCTION (swtch.ipv4 << xid);

    NS_LOG_INFO ("Received packetIn from node " << swtch.node->GetId ());

    // TODO

    // static int prio = 100;
    // uint32_t outPort = OFPP_FLOOD;
    
    uint64_t dpId = swtch.swDev->GetDatapathId ();
    enum ofp_packet_in_reason reason = msg->reason;

    char *m = ofl_structs_match_to_string ((struct ofl_match_header*)msg->match, 0);
    NS_LOG_DEBUG ("Packet in match: " << m);
    free (m);

    NS_LOG_INFO ("msg->buffer_id == " << msg->buffer_id);
    NS_LOG_INFO ("msg->data_length == " << msg->data_length);
 
    // ofl_msg_unpack_packet_in sets msg->data to NULL if
    // msg->data_length <= 0. Path in ofswitch13-device.cc
    // if (msg->data != NULL)
    // {
    //     NS_LOG_INFO ("msg->data != NULL");
        
    //     // Retrieve the packet included in packet_in
    //     Packet packet = Packet (msg->data, msg->data_length);
    //     EthernetTrailer ethernetTrailer;
    //     packet.RemoveTrailer (ethernetTrailer);
    //     EthernetHeader ethernetHeader;
    //     packet.RemoveHeader (ethernetHeader);
    //     // LlcSnapHeader llcHeader;
    //     // packet.RemoveHeader (llcHeader);
    //     // TopologyHeader topoHeader;
    //     // packet.RemoveHeader (topoHeader);
    //     Ipv4Header ipv4Header;
    //     packet.RemoveHeader (ipv4Header);
    //     UdpHeader udpHeader;
    //     packet.RemoveHeader (udpHeader);
        
    //     TopologyHeader topologyHeader;
    //     packet.RemoveHeader (topologyHeader);
    //     NS_LOG_INFO ("RETRIEVED PACKET (" << packet.GetSize () << " bytes) - " << topologyHeader);
    // }


    if (reason == OFPR_NO_MATCH)
    {
        NS_LOG_INFO ("reason == OFPR_NO_MATCH");
        // Let's get necessary information (input port and mac address)
        uint32_t inPort;
        size_t portLen = OXM_LENGTH (OXM_OF_IN_PORT); // (Always 4 bytes)
        ofl_match_tlv *input = oxm_match_lookup (OXM_OF_IN_PORT,
                                                 (ofl_match*)msg->match);
        memcpy (&inPort, input->value, portLen);

        Mac48Address src48;
        ofl_match_tlv *ethSrc = oxm_match_lookup (OXM_OF_ETH_SRC,
                                                  (ofl_match*)msg->match);
        src48.CopyFrom (ethSrc->value);

        Mac48Address dst48;
        ofl_match_tlv *ethDst = oxm_match_lookup (OXM_OF_ETH_DST,
                                                  (ofl_match*)msg->match);
        dst48.CopyFrom (ethDst->value);

        NS_LOG_INFO ("Port " << inPort << " - Source " << src48 << " - Dest " << dst48);

     // if (msg->data != NULL)
     //    {
     //        NS_LOG_INFO ("msg->data != NULL");
            
     //        // Retrieve the packet included in packet_in
     //        Packet packet = Packet (msg->data, msg->data_length);
     //        EthernetTrailer ethernetTrailer;
     //        packet.RemoveTrailer (ethernetTrailer);
     //        EthernetHeader ethernetHeader;
     //        packet.RemoveHeader (ethernetHeader);
     //        // LlcSnapHeader llcHeader;
     //        // packet.RemoveHeader (llcHeader);
     //        // TopologyHeader topoHeader;
     //        // packet.RemoveHeader (topoHeader);
     //        Ipv4Header ipv4Header;
     //        packet.RemoveHeader (ipv4Header);
     //        UdpHeader udpHeader;
     //        packet.RemoveHeader (udpHeader);
            
     //        TopologyHeader topologyHeader;
     //        packet.RemoveHeader (topologyHeader);
     //        NS_LOG_INFO ("RETRIEVED PACKET (" << packet.GetSize () << " bytes) - " << topologyHeader);
     //    }

    //     // Get L2Table for this datapath
    //     DatapathMap_t::iterator it = m_learnedInfo.find (dpId);
    //     if (it != m_learnedInfo.end ())
    //       {
    //         L2Table_t *l2Table = &it->second;

    //         // Looking for out port based on dst address (except for broadcast)
    //         if (!dst48.IsBroadcast ())
    //           {
    //             L2Table_t::iterator itDst = l2Table->find (dst48);
    //             if (itDst != l2Table->end ())
    //               {
    //                 outPort = itDst->second;
    //               }
    //             else
    //               {
    //                 NS_LOG_DEBUG ("No L2 switch information for mac " << dst48 <<
    //                               " yet. Flooding...");
    //               }
    //           }

    //         // Learning port from source address
    //         NS_ASSERT_MSG (!src48.IsBroadcast (), "Invalid src broadcast addr");
    //         L2Table_t::iterator itSrc = l2Table->find (src48);
    //         if (itSrc == l2Table->end ())
    //           {
    //             std::pair <L2Table_t::iterator, bool> ret;
    //             ret = l2Table->insert (
    //                 std::pair<Mac48Address, uint32_t> (src48, inPort));
    //             if (ret.second == false)
    //               {
    //                 NS_LOG_ERROR ("Can't insert mac48address / port pair");
    //               }
    //             else
    //               {
    //                 NS_LOG_DEBUG ("Learning that mac " << src48 <<
    //                               " can be found at port " << inPort);

    //                 // Send a flow-mod to switch creating this flow. Let's
    //                 // configure the flow entry to 10s idle timeout and to
    //                 // notify the controller when flow expires. (flags=0x0001)
    //                 std::ostringstream cmd;
    //                 cmd << "flow-mod cmd=add,table=0,idle=10,flags=0x0001"
    //                     << ",prio=" << ++prio << " eth_dst=" << src48
    //                     << " apply:output=" << inPort;
    //                 DpctlCommand (swtch, cmd.str ());
    //               }
    //           }
    //         else
    //           {
    //             NS_ASSERT_MSG (itSrc->second == inPort,
    //                            "Inconsistent L2 switching table");
    //           }
    //       }
    //     else
    //       {
    //         NS_LOG_WARN ("No L2Table for this datapath id " << dpId);
    //       }

    //     // Lets send the packet out to switch.
    //     ofl_msg_packet_out reply;
    //     reply.header.type = OFPT_PACKET_OUT;
    //     reply.buffer_id = msg->buffer_id;
    //     reply.in_port = inPort;
    //     reply.data_length = 0;
    //     reply.data = 0;


        // OfSwitch13Device implementations save the ns3 packet id as
        // buffer field, even if miss_send_len == OFPCML_NO_BUFFER.
        
        // if (msg->buffer_id == NO_BUFFER)
        // {
        //     // No packet buffer. Send data back to switch
        //     reply.data_length = msg->data_length;
        //     reply.data = msg->data;
        //     NS_LOG_INFO ("msg->buffer_id == NO_BUFFER ");
        // }

        // NS_LOG_INFO ("msg->buffer_id == " << msg->buffer_id);
        // NS_LOG_INFO ("msg->data_length == " << msg->data_length);
        // NS_LOG_INFO ("msg->data == " << msg->data);
        
        // if (msg->data_length > 0)
        // {
        //     NS_LOG_INFO ("msg->data == " << msg->data);
        // }

    //     // Create output action
    //     ofl_action_output *a =
    //       (ofl_action_output*)xmalloc (sizeof (struct ofl_action_output));
    //     a->header.type = OFPAT_OUTPUT;
    //     a->port = outPort;
    //     a->max_len = 0;

    //     reply.actions_num = 1;
    //     reply.actions = (ofl_action_header**)&a;

    //     SendToSwitch (&swtch, (ofl_msg_header*)&reply, xid);
    //     free (a);
    }
    else if (reason == OFPR_ACTION)
    {
        NS_LOG_INFO ("reason == OFPR_ACTION");
        
        Mac48Address src48;
        ofl_match_tlv *ethSrc = oxm_match_lookup (OXM_OF_ETH_SRC,
                                                  (ofl_match*)msg->match);
        src48.CopyFrom (ethSrc->value);
        
        // TopologyHeader topologyHeader;
        if (msg->data != NULL)
        {
            NS_LOG_INFO ("msg->data != NULL");
            
            // Retrieve the packet included in packet_in
            Packet packet = Packet (msg->data, msg->data_length);
            
            // EthernetTrailer ethernetTrailer;
            // packet.RemoveTrailer (ethernetTrailer);
            // EthernetHeader ethernetHeader;
            // packet.RemoveHeader (ethernetHeader);
            // // LlcSnapHeader llcHeader;
            // // packet.RemoveHeader (llcHeader);
            // // TopologyHeader topoHeader;
            // // packet.RemoveHeader (topoHeader);
            // Ipv4Header ipv4Header;
            // packet.RemoveHeader (ipv4Header);
            // UdpHeader udpHeader;
            // packet.RemoveHeader (udpHeader);
            
            // // TopologyHeader topologyHeader;
            // packet.RemoveHeader (topologyHeader);
            // NS_LOG_INFO ("RETRIEVED PACKET (" << packet.GetSize () << " bytes) - " << topologyHeader);

            EthernetTrailer ethernetTrailer;
            packet.RemoveTrailer (ethernetTrailer);
            EthernetHeader ethernetHeader;
            packet.RemoveHeader (ethernetHeader);
            Ipv4Header ipv4Header;
            packet.RemoveHeader (ipv4Header);
            UdpHeader udpHeader;
            packet.RemoveHeader (udpHeader);
            TypeHeader typeHeader;
            packet.RemoveHeader (typeHeader);
            // ReportHeader reportHeader;
            // packet.RemoveHeader (reportHeader);
            UpdateHeader updateHeader;
            packet.RemoveHeader (updateHeader);

            NS_LOG_INFO ("RETRIEVED PACKET from " << dpId << " (" << packet.GetSize () << " bytes) - " << typeHeader << "\n" << updateHeader);

        }

        // // TODO: store received information
        // DatapathMap_t::iterator it = m_learnedInfo.find (dpId);
        // if (it != m_learnedInfo.end ())
        // {
        //     // Retrieve NodeInfoTable associated with the current RSU
        //     NodeInfoTable_t *nodeInfoTable = &it->second;
        //     // Retrieve the entry related to the src48
        //     NodeInfoTable_t::iterator itInfo = nodeInfoTable->find (src48);
        //     if (itInfo != nodeInfoTable->end ())
        //     {
        //         // Not the first position update from the node
        //         NS_LOG_INFO ("Add node info for " << src48);
        //     }
        //     else
        //     {
        //         // First position update from the node
        //         NS_LOG_INFO ("Add FIRST node info for " << src48);
        //     }

        //     NodeInfo_t nodeInfo;
        //     nodeInfo.x = topologyHeader.GetNodeInfoX ();
        //     nodeInfo.y = topologyHeader.GetNodeInfoY ();
        //     nodeInfo.speed = topologyHeader.GetNodeInfoSpeed ();

        //     // Add the new entry
        //     std::pair <NodeInfoTable_t::iterator, bool> res;
        //     res = nodeInfoTable->insert (
        //           std::pair<Mac48Address, NodeInfo_t> (src48, nodeInfo));

        //     if (res.second == false)
        //     {
        //         // TODO
        //         NS_LOG_INFO ("Overriding entry of node " << src48);
        //     }

        //     PrintNodesInformation ();
        // }
        // else
        // {
        //     NS_LOG_WARN ("No NodeInfoTable for this datapath id " << dpId);
        // }
    }
    else
    {
        NS_LOG_WARN ("This controller can't handle the packet. Unkwnon reason.");
    }

    // All handlers must free the message when everything is ok
    ofl_msg_free ((ofl_msg_header*)msg, 0);
    return 0;
}

/**
 * ---> Private methods <---
 */
void
OFSwitch13TopologyController::ConnectionStarted (SwitchInfo swtch)
{
    NS_LOG_FUNCTION (this << swtch.ipv4);


    // NOTE: here we can assume knowledge about RSUs positions and
    // addresses and leverage it for configuration


    // NOTE The miss config DOESN'T MATTER. Each switch is gonna send always the
    // entire packet.
    
    // Configure the switch to send enough bytes to contain the entire packet
    std::ostringstream cmd;
    cmd << "set-config miss=" << OFPCML_NO_BUFFER;
    // cmd << "set-config miss=128"; 
    // cmd << "get-config";
    DpctlCommand (swtch, cmd.str ());

    // After a successfull handshake, let's install the table-miss entry.
    // In this way, OFPR_NO_MATCH happens at the controller
    DpctlCommand (swtch, "flow-mod cmd=add,table=0,prio=0, apply:output=ctrl");
    // otherwise we can install en explicit rule for sending to the controller
    // only packets received from wireless interface (port 1 in ofswitch13-device).
    // In this way, OFPR_ACTION happens at the controller
    // DpctlCommand (swtch, "flow-mod cmd=add,table=0,prio=0,hard=1000 in_port=1 apply:output=ctrl");
    DpctlCommand (swtch, "flow-mod cmd=add,table=0,prio=20 in_port=1 apply:output=ctrl");

    // We can also selectively configure each RSU according to its position
    // if (swtch.ipv4.IsEqual (Ipv4Address ("10.100.150.5")))
    // {
    //     // Forwarding wireless -> wired
    //     DpctlCommand (swtch, "flow-mod cmd=add,table=0,prio=500,hard=200 in_port=1 apply:output=2");
    // }

    // if (swtch.ipv4.IsEqual (Ipv4Address ("10.100.150.1")))
    // {
    //     // Forwarding wired -> wireless
    //     DpctlCommand (swtch, "flow-mod cmd=add,table=0,prio=500,hard=200 in_port=2 apply:output=1");
    // }




    // DpctlCommand (swtch, "flow-mod cmd=add,table=0,prio=20 "
    //                      "in_port=1, apply:output=ctrl");
    // DpctlCommand (swtch, "flow-mod cmd=add,table=0,prio=20 "
    //                      "in_port=2, apply:output=ctrl");
    
    // Configure the switch to buffer packets and send only the first 128 bytes
    // std::ostringstream cmd;
    // cmd << "set-config miss=128";
    // DpctlCommand (swtch, cmd.str ());

    // DpctlCommand (swtch, "port-desc");


    // Create an empty NodeInfoTable and insert it into m_learnedInfo
    NodeInfoTable_t nodeInfoTable;
    uint64_t dpId = swtch.swDev->GetDatapathId ();

    // Create a DatapathMap_t entry for each connection started
    std::pair <DatapathMap_t::iterator, bool> ret;
    ret =  m_learnedInfo.insert (std::pair<uint64_t, NodeInfoTable_t> (dpId, nodeInfoTable));
    if (ret.second == false)
    {
        NS_LOG_ERROR ("There is already a table for this datapath");
    }
}

void
OFSwitch13TopologyController::PrintNodesInformation ()
{
    NS_LOG_FUNCTION (this);
    NS_LOG_INFO ("Printing nodes information...");

    std::ofstream outFile;
    std::ostringstream stream;
    
    // outFile.open ("nodes.info", std::ios_base::app);
    std::ostringstream fileName;
    fileName << "nodes" << Simulator::Now ().GetSeconds () << ".info";
    outFile.open (fileName.str ().c_str ());

    DatapathMap_t::iterator it;
    for (DatapathMap_t::iterator it = m_learnedInfo.begin (); it != m_learnedInfo.end (); it++)
    {
        NS_LOG_INFO ("m_learnedInfo size=" << m_learnedInfo.size ());
        uint64_t dpId = it->first;
        // stream << dpId;

        NodeInfoTable_t *nodeInfoTable = &it->second;
        NS_LOG_INFO ("nodeInfoTable size=" << nodeInfoTable->size ());
        for (NodeInfoTable_t::iterator itInfo = nodeInfoTable->begin ();
             itInfo != nodeInfoTable->end (); itInfo++)
        {
            Mac48Address src48 = itInfo->first;

            // [DatapathId]  [src48]        [info.x]    [info.y]    [info.speed]
            stream << dpId;
            stream << "\t" << src48;
            NodeInfo_t nodeInfo = itInfo->second;
            stream << "\t\t" << nodeInfo.x 
                   << "\t\t" << nodeInfo.y
                   << "\t\t" << nodeInfo.speed << std::endl;
        }

        // stream << std::endl;
    }

    outFile << stream.str () << std::endl;
    outFile.close ();
    NS_LOG_INFO ("Printing done");
}

} // namespace ns3
// #endif // NS3_OFSWITCH13
