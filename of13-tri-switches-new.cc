/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 University of Campinas (Unicamp)
 *
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
 *
 * Two hosts connected to a single OpenFlow switch.
 * The switch is managed by to different controllers applications (Controllers 0 and 1).
 *
 *                          +----------+
 *                          | Switch３ | === Host ３
 *                          +----------+
 *            　　　　　　          |
 *                                |
 *                       　　　Controller
 *                                |
 *                         +-------------+
 *                         |             |
 *                  +----------+     +----------+
 *       Host 0 === | Switch 0 | === | Switch 1 | === Host 1
 *                  +----------+     +----------+
 *　　　　　　　　　　　

 */
 */

#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/csma-module.h>
#include <ns3/internet-module.h>
#include <ns3/ofswitch13-module.h>
#include <ns3/internet-apps-module.h>
#include <ns3/netanim-module.h>
#include <ns3/application.h>
#include <ns3/socket.h>
#include <string>
int i=0;
int j=0;
using namespace ns3;

class Controller0;
class Controller1;
class Controller2;
AnimationInterface * pAnim = 0;

/// RGB structure
struct rgb {
  uint8_t r; ///< red
  uint8_t g; ///< green
  uint8_t b; ///< blue
};

struct rgb colors [] = {
                        { 255, 0, 0 }, // Red
                        { 0, 255, 0 }, // Blue
                        { 0, 0, 255 }  // Green
                        };

void modify ()
{
  std::ostringstream oss;
  oss << "Update:" << Simulator::Now ().GetSeconds ();
  // Every update change the node description for node 2
  std::ostringstream node0Oss;
  node0Oss << "-----Node:" << Simulator::Now ().GetSeconds ();
  if (Simulator::Now ().GetSeconds () < 10) // This is important or the simulation
    // will run endlessly
    Simulator::Schedule (Seconds (1), modify);

}
int
main (int argc, char *argv[])
{
  uint16_t simTime = 10;
  bool verbose = false;
  bool trace = false;

  // Configure command line parameters
  CommandLine cmd;
  cmd.AddValue ("simTime", "Simulation time (seconds)", simTime);
  cmd.AddValue ("verbose", "Enable verbose output", verbose);
  cmd.AddValue ("trace", "Enable datapath stats and pcap traces", trace);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      OFSwitch13Helper::EnableDatapathLogs ();
      LogComponentEnable ("OFSwitch13Interface", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Device", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Port", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Queue", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13SocketHandler", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Controller", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13LearningController", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Helper", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13InternalHelper", LOG_LEVEL_ALL);
    }

  // Enable checksum computations (required by OFSwitch13 module)
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  // Create two host nodes
  NodeContainer hosts;
  hosts.Create (3);

  // Create the switch node
  NodeContainer switches;
  switches.Create(3);

  // Use the CsmaHelper to connect host nodes to the switch node
  CsmaHelper csmaHelper;
  csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csmaHelper.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer hostDevices;
  NetDeviceContainer switchPorts[3];
  for (size_t i = 0; i < hosts.GetN (); i++)
    {
      NodeContainer pair (hosts.Get (i), switches.Get(i));
      NetDeviceContainer link = csmaHelper.Install (pair);
      hostDevices.Add (link.Get (0));
      switchPorts[i].Add (link.Get (1));
    }

  NodeContainer pair1 (switches.Get (0), switches.Get(1));
  NetDeviceContainer link1 = csmaHelper.Install (pair1);
  switchPorts[0].Add (link1.Get (0));
  switchPorts[1].Add (link1.Get (1));
  NodeContainer pair2 (switches.Get (1), switches.Get(2));
  NetDeviceContainer link2 = csmaHelper.Install (pair2);
  switchPorts[1].Add (link2.Get (0));
  switchPorts[2].Add (link2.Get (1));
  NodeContainer pair3 (switches.Get (2), switches.Get(0));
  NetDeviceContainer link3 = csmaHelper.Install (pair3);
  switchPorts[2].Add (link3.Get (0));
  switchPorts[0].Add (link3.Get (1));
  // Create two controller nodes
  NodeContainer controllers;
  controllers.Create (1);

  // Configure the OpenFlow network domain
  Ptr<OFSwitch13InternalHelper> of13Helper0 = CreateObject<OFSwitch13InternalHelper> ();
  Ptr<Controller0> ctrl0 = CreateObject<Controller0> ();

  of13Helper0->InstallController (controllers.Get (0), ctrl0);
  of13Helper0->InstallSwitch (switches.Get(0), switchPorts[0]);
  of13Helper0->InstallSwitch (switches.Get(1), switchPorts[1]);
  of13Helper0->InstallSwitch (switches.Get(2), switchPorts[2]);
  of13Helper0->CreateOpenFlowChannels ();

  // Install the TCP/IP stack into hosts nodes
  InternetStackHelper internet;
  internet.Install (hosts);

  // Set IPv4 host addresses
  Ipv4AddressHelper ipv4helpr;
  Ipv4InterfaceContainer hostIpIfaces;
  ipv4helpr.SetBase ("10.1.1.0", "255.255.255.0");
  hostIpIfaces = ipv4helpr.Assign (hostDevices);

  // Configure ping application between hosts
  V4PingHelper pingHelper = V4PingHelper (hostIpIfaces.GetAddress (2));
  pingHelper.SetAttribute ("Verbose", BooleanValue (true));
  ApplicationContainer pingApps = pingHelper.Install (hosts.Get (0));
  pingApps.Start (Seconds (1));


  // Enable datapath stats and pcap traces at hosts, switch(es), and controller(s)
  if (trace)
    {
//      of13Helper->EnableOpenFlowPcap ("openflow");
//      of13Helper->EnableDatapathStats ("switch-stats");
//      csmaHelper.EnablePcap ("switch", switchPorts[0], true);
//      csmaHelper.EnablePcap ("switch", switchPorts[1], true);
//      csmaHelper.EnablePcap ("switch", switchPorts[2], true);
//      csmaHelper.EnablePcap ("host", hostDevices);
    }
  	AnimationInterface::SetConstantPosition(hosts.Get(0),14.5,25);
    AnimationInterface::SetConstantPosition(hosts.Get(1),65.5,25);
    AnimationInterface::SetConstantPosition(hosts.Get(2),40,70);
    AnimationInterface::SetConstantPosition(switches.Get(0),23,30);
    AnimationInterface::SetConstantPosition(switches.Get(1),57,30);
    AnimationInterface::SetConstantPosition(switches.Get(2),40,60);
    AnimationInterface::SetConstantPosition(controllers.Get(0),40,40);



  // Run the simulation
  Simulator::Stop (Seconds (simTime));
  pAnim=new AnimationInterface("my-of13.xml");
  pAnim->EnablePacketMetadata(true);
  Simulator::Schedule (Seconds (1), modify);
  Simulator::Run ();
  Simulator::Destroy ();

}

class Controller0 : public OFSwitch13Controller
{
protected:
	void
		HandshakeSuccessful (Ptr<const RemoteSwitch> swtch)
		{

			if((int)swtch->GetDpId()==1){
				printf("%d",(int)swtch->GetDpId());
				DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=2 in_port=2 apply:output=1");
				DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=3 in_port=1 apply:output=2");
			}
			if((int)swtch->GetDpId()==2){
				printf("%d",(int)swtch->GetDpId());
				DpctlExecute (swtch, "flow-mod cmd=add,table=0,flags=0x0001,prio=2 in_port=2 write:output=3");
				DpctlExecute (swtch, "flow-mod cmd=add,table=0,flags=0x0001,prio=1 in_port=3 write:output=2");
			}
			if((int)swtch->GetDpId()==3){
				printf("%d",(int)swtch->GetDpId());
				DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=2 in_port=0 write:output=1");
				DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=1 in_port=0 write:output=2");
			}
		}

		ofl_err HandleFlowRemoved (
		    struct ofl_msg_flow_removed *msg, Ptr<const RemoteSwitch> swtch,
		    uint32_t xid){
			if((int)swtch->GetDpId()==1){
				if(j==1){
					DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=2 in_port=3 apply:output=1");
					DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=1 in_port=1 apply:output=3");
					j=0;
				}else{
					j=1;
					DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=2 in_port=2 apply:output=1");
					DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=1 in_port=1 apply:output=2");
				}
			}
			if((int)swtch->GetDpId()==3){
				if(i==1){
					DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=2 in_port=3 write:output=1");
					DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=1 in_port=1 write:output=3");
					i=0;
				}else{
					i=1;
					DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=2 in_port=2 write:output=1");
					DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=1 in_port=1 write:output=2");
				}
			}
			return 0;
		}
};
