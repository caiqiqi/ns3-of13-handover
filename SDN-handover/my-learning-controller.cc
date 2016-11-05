/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Luciano Chaves <luciano@lrc.ic.unicamp.br>
 */

#ifdef NS3_OFSWITCH13

#include "my-learning-controller.h"

NS_LOG_COMPONENT_DEFINE ("MyLearningController");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (MyLearningController);

/********** Public methods ***********/
MyLearningController::MyLearningController ()
{
  NS_LOG_FUNCTION (this);
}

MyLearningController::~MyLearningController ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
MyLearningController::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MyLearningController")
    .SetParent<OFSwitch13Controller> ()
    .SetGroupName ("OFSwitch13")
    .AddConstructor<MyLearningController> ()
  ;
  return tid;
}

void
MyLearningController::DoDispose ()
{
  m_learnedInfo.clear ();
  OFSwitch13Controller::DoDispose ();
}

/*
处理从switch发到controller的 packet_in 消息!!!
*/
ofl_err
MyLearningController::HandlePacketIn (ofl_msg_packet_in *msg,
                                              SwitchInfo swtch, uint32_t xid)
{
  NS_LOG_FUNCTION (swtch.ipv4 << xid);

  static int prio = 100;   //优先级
  uint32_t outPort = OFPP_FLOOD;   // 默认是向所有端口泛洪
  uint64_t dpId = swtch.swDev->GetDatapathId ();  // 得到这个消息的datapath ID
  enum ofp_packet_in_reason reason = msg->reason;  //packet_in 的reason

  char *m =
    ofl_structs_match_to_string ((struct ofl_match_header*)msg->match, 0);
  NS_LOG_DEBUG ("Packet in match: " << m);
  free (m);
// "OFPR_NO_MATCH" 表示controller没有找到匹配的flow entry (no matching flow entry is found)
  if (reason == OFPR_NO_MATCH)
    {
      // Let's get necessary information (input port and mac address)
      uint32_t inPort;
      // 入switch的端口号(Always 4 bytes)
      // 最开始是从AP1来的，input为1，然后input分别为2, 3
      size_t portLen = OXM_LENGTH (OXM_OF_IN_PORT);
      ofl_match_tlv *input = oxm_match_lookup (OXM_OF_IN_PORT,
                                               (ofl_match*)msg->match);
      // 将input的值的portLen位写到inPort的值中
      memcpy (&inPort, input->value, portLen);

      Mac48Address src48;  // 源物理地址
      ofl_match_tlv *ethSrc = oxm_match_lookup (OXM_OF_ETH_SRC,
                                                (ofl_match*)msg->match);
      src48.CopyFrom (ethSrc->value);

      Mac48Address dst48;  // 目的物理地址
      ofl_match_tlv *ethDst = oxm_match_lookup (OXM_OF_ETH_DST,
                                                (ofl_match*)msg->match);
      dst48.CopyFrom (ethDst->value);

      // Get L2Table for this datapath
      // DatapthMap_t 为 datapathId 到 L2Table_t的映射
      DatapathMap_t::iterator it = m_learnedInfo.find (dpId);
      // 在 m_learnedInfo 的可用内容中
      if (it != m_learnedInfo.end ())
        {
          L2Table_t *l2Table = &it->second;

          // Looking for out port based on dst address (except for broadcast)
          // 从l2Table中根据目的物理地址找out port(目的物理地址 dst48 不能是广播地址)
          if (!dst48.IsBroadcast ())
            {
              L2Table_t::iterator itDst = l2Table->find (dst48);
              /*
              若在 l2Table 中找打了这个目的物理地址，则将查到的该物理地址对应的switch端口作为为output的端口；
              否则，打印出未找到该物理地址，并泛洪
              */
              if (itDst != l2Table->end ())
                {
                  outPort = itDst->second;
                }
              else
                {
                  NS_LOG_DEBUG ("No L2 switch information for mac " << dst48 <<
                                " yet. Flooding...");
                }
            }

          // Learning port from source address
          NS_ASSERT_MSG (!src48.IsBroadcast (), "Invalid src broadcast addr");//确保不是广播
          L2Table_t::iterator itSrc = l2Table->find (src48);
          /*
          若在 l2Table 中未找到这个源物理地址，则将该物理地址插入到l2Table中以学习
          */
          if (itSrc == l2Table->end ())
            {
              std::pair <L2Table_t::iterator, bool> ret;
              ret = l2Table->insert (
                  std::pair<Mac48Address, uint32_t> (src48, inPort));
              if (ret.second == false)
                {
                  NS_LOG_ERROR ("Can't insert mac48address / port pair");
                }
              else
                {
                  NS_LOG_DEBUG ("Learning that mac " << src48 <<
                                " can be found at port " << inPort);

                  // Send a flow-mod to switch creating this flow. Let's
                  // configure the flow entry to 10s idle timeout and to
                  // notify the controller when flow expires. (flags=0x0001)
                  std::ostringstream cmd;
                  cmd << "flow-mod cmd=add,table=0,idle=2,flags=0x0001"
                      << ",prio=" << ++prio << " eth_dst=" << src48
                      << " apply:output=" << inPort;
                  DpctlCommand (swtch, cmd.str ());
                }
            }
          else
            {
              // commented by CQQ
              //NS_ASSERT_MSG (itSrc->second == inPort, "Inconsistent L2 switching table");
            }
        }
      // 若不在 m_learnedInfo 的内容中
      else
        {
          NS_LOG_WARN ("No L2Table for this datapath id " << dpId);
        }

      // Lets send the packet out to switch.
      ofl_msg_packet_out reply;
      reply.header.type = OFPT_PACKET_OUT;
      reply.buffer_id = msg->buffer_id;
      reply.in_port = inPort;
      reply.data_length = 0;
      reply.data = 0;

      if (msg->buffer_id == NO_BUFFER)
        {
          // No packet buffer. Send data back to switch
          reply.data_length = msg->data_length;
          reply.data = msg->data;
        }

      // Create output action
      ofl_action_output *a =
        (ofl_action_output*)xmalloc (sizeof (struct ofl_action_output));
      a->header.type = OFPAT_OUTPUT;
      a->port = outPort;
      a->max_len = 0;

      reply.actions_num = 1;
      reply.actions = (ofl_action_header**)&a;

      SendToSwitch (&swtch, (ofl_msg_header*)&reply, xid);
      free (a);
    }
  else
    {
      NS_LOG_WARN ("This controller can't handle the packet. Unkwnon reason.");
    }

  // All handlers must free the message when everything is ok
  ofl_msg_free ((ofl_msg_header*)msg, 0);
  return 0;
}

ofl_err
MyLearningController::HandleFlowRemoved (
  ofl_msg_flow_removed *msg, SwitchInfo swtch, uint32_t xid)
{
  NS_LOG_FUNCTION (swtch.ipv4 << xid);
  NS_LOG_DEBUG ( "Flow entry expired. Removing from L2 switch table.");

  uint64_t dpId = swtch.swDev->GetDatapathId ();
  DatapathMap_t::iterator it = m_learnedInfo.find (dpId);
  if (it != m_learnedInfo.end ())
    {
      Mac48Address mac48;
      ofl_match_tlv *ethSrc =
        oxm_match_lookup (OXM_OF_ETH_DST, (ofl_match*)msg->stats->match);
      mac48.CopyFrom (ethSrc->value);

      L2Table_t *l2Table = &it->second;
      L2Table_t::iterator itSrc = l2Table->find (mac48);
      if (itSrc != l2Table->end ())
        {
          l2Table->erase (itSrc);
        }
    }

  // All handlers must free the message when everything is ok
  ofl_msg_free_flow_removed (msg, true, 0);
  return 0;
}

/********** Private methods **********/
void
MyLearningController::ConnectionStarted (SwitchInfo swtch)
{
  NS_LOG_FUNCTION (this << swtch.ipv4);

  // After a successfull handshake, let's install the table-miss entry
  DpctlCommand (swtch, "flow-mod cmd=add,table=0,prio=0 apply:output=ctrl");

  // Configure the switch to buffer packets and send only the first 128 bytes
  std::ostringstream cmd;
  cmd << "set-config miss=128";
  DpctlCommand (swtch, cmd.str ());

  // Create an empty L2SwitchingTable and insert it into m_learnedInfo
  L2Table_t l2Table;
  uint64_t dpId = swtch.swDev->GetDatapathId ();

  std::pair <DatapathMap_t::iterator, bool> ret;
  ret =  m_learnedInfo.insert (std::pair<uint64_t, L2Table_t> (dpId, l2Table));
  if (ret.second == false)
    {
      NS_LOG_ERROR ("There is already a table for this datapath");
    }
}

} // namespace ns3
#endif // NS3_OFSWITCH13
