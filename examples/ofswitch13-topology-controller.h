/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Author: Mattia Bisacchi <mattia.bisacchi@studio.unibo.it> 
 */

#ifndef OFSWITCH13_TOPOLOGY_CONTROLLER_H
#define OFSWITCH13_TOPOLOGY_CONTROLLER_H

#include "ns3/ofswitch13-interface.h"
#include "ns3/ofswitch13-device.h"
#include "ns3/ofswitch13-controller.h"

#include "topology-types.h"


namespace ns3 {

/*
 * \brief TODO
 */
class OFSwitch13TopologyController : public OFSwitch13Controller
{
    public:
        // Default constructor
        OFSwitch13TopologyController ();
        // Dummy destructor
        virtual ~OFSwitch13TopologyController ();

        static TypeId GetTypeId (void);
        // Destructor implementation
        virtual void DoDispose ();

        /**
         * Handle packet-in messages sent from the switch to this controller.
         * TODO: complete behavior description. 
         *
         * \return 0 if everthing's ok, otherwise an error number.
         */
        ofl_err HandlePacketIn (ofl_msg_packet_in *msg, SwitchInfo swtch,
                                uint32_t xid);
    protected:
        // Inherited from OFSwitch13Controller
        void ConnectionStarted (SwitchInfo swtch);

    private:
        void PrintNodesInformation ();

        /*
         * +-------+--------------+-------------+
         * | RSUid |       NodeInfoTable_t      | 
         * |       |----------------------------|         
         * |       |              | NodeInfo_t  |
         * |       | Mac48Address |-------------|
         * |       |              | x | y | spd |
         * *-------+--------------+-------------+
         * .               ...                  .
         * .               ...                  .
         * .               ...                  .
         * +-------+--------------+-------------+
         * |       |              |             |
         * *-------+--------------+-------------+
         **/

        /*
         * DatapathMap_t
         *  |
         *  +-> std::map<uint64_t, NodeInfoTable_t>
         *               ^^^^^^^^   |
         *                          |
         *                          +-> std::map<Mac48Address, NodeInfo_t>
         *                                       ^^^^^^^^^^^^   |
         *                                                      |
         *                                                      +-> int x
         *                                                      |       ^
         *                                                      |
         *                                                      +-> int y
         *                                                      |       ^
         *                                                      |
         *                                                      +-> int speed
         *                                                              ^^^^^
         *
         */

        // typedef struct NodeInfo_t {
        //     uint32_t x;
        //     uint32_t y;
        //     uint32_t speed;
        // } NodeInfo_t;

        // NodeInfoTable: map Mac48Address to node information
        typedef std::map<Mac48Address, NodeInfo_t> NodeInfoTable_t;

        // Map datapathID (RSU id) to NodeInfoTable
        typedef std::map<uint64_t, NodeInfoTable_t> DatapathMap_t;

        // Nodes position information
        DatapathMap_t m_learnedInfo;
};

} // namespace ns3
#endif /* OFSWITCH13_TOPOLOGY_CONTROLLER_H */