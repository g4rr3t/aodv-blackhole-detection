/* Simulation 2: Test Black hole attack
 * 
 * Network topology
 *             
 *            n6 (black hole)
 *             |
 *            n5
 *             |
 *            n4
 *             |
 *    n0 ---> n1 ---> n2 ---> n3
 * 
 * Each node is only in the range of its immediate adjacent.
 * Source Node: n0
 * Destination Node: n3
 * Black hole Node : n6
 *
 * Scenario:
 * n0 will request a route to n3, however n6 (black hole) will pose as n3 and steal the route
 * All UDP packets will route to n6 and will be dropped
 *
 * Output of this file:
 * 1. Generates sim2.routes file for routing table information and
 * 2. Sim2Animation.xml file for viewing animation in NetAnim.
 * 
 *
 *  Simulation pass criteria:
 *    Flowmonitor reports 0% packets were transmitted from n0 to n3
 *
 *  Run script with command ./waf --run simulation2
 */
#include "ns3/aodv-module.h"
#include "ns3/netanim-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mobility-module.h"
#include "normalapp.h"

NS_LOG_COMPONENT_DEFINE ("Blackhole");

using namespace ns3;

void
ReceivePacket(Ptr<const Packet> p, const Address & addr)
{
	std::cout << Simulator::Now ().GetSeconds () << "\t" << p->GetSize() <<"\n";
}


int main (int argc, char *argv[])
{
  bool enableFlowMonitor = false;
  std::string phyMode ("DsssRate1Mbps");

// Setup flow monitor
  CommandLine cmd;
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.Parse (argc, argv);

// Explicitly create the nodes required by the topology (described above).
  NS_LOG_INFO ("Create nodes.");
  NodeContainer c; // ALL Nodes
  NodeContainer not_malicious;
  NodeContainer blackhole;
  c.Create(7);

  not_malicious.Add(c.Get(0));
  not_malicious.Add(c.Get(1));
  not_malicious.Add(c.Get(2));
  not_malicious.Add(c.Get(3));
  not_malicious.Add(c.Get(4));
  not_malicious.Add(c.Get(5));
  blackhole.Add(c.Get(6));
  
  // Set up WiFi
  WifiHelper wifi;

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11);

  YansWifiChannelHelper wifiChannel ;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::TwoRayGroundPropagationLossModel",
	  	  	  	  	  	  	  	    "SystemLoss", DoubleValue(1),
		  	  	  	  	  	  	    "HeightAboveZ", DoubleValue(1.5));

  // For range near 250m
  wifiPhy.Set ("TxPowerStart", DoubleValue(33));
  wifiPhy.Set ("TxPowerEnd", DoubleValue(33));
  wifiPhy.Set ("TxPowerLevels", UintegerValue(1));
  wifiPhy.Set ("TxGain", DoubleValue(0));
  wifiPhy.Set ("RxGain", DoubleValue(0));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue(-61.8));
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue(-64.8));

  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add a non-QoS upper mac
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  //wifiMac.SetType ("ns3::AdhocWifiMac");

  // Set 802.11b standard
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue(phyMode),
                                "ControlMode",StringValue(phyMode));


  NetDeviceContainer devices;
  devices = wifi.Install (wifiPhy, wifiMac, c);


//  Enable AODV
  AodvHelper aodv;
  AodvHelper blackhole_aodv; 

// turn off hello transmissions that confuse the logs
  aodv.Set("EnableHello", BooleanValue (false));
  blackhole_aodv.Set("EnableHello", BooleanValue (false));

  // Set up internet stack
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (not_malicious);

  // putting *false* instead of *true* would disable the malicious behavior of the node
  blackhole_aodv.Set("IsMalicious",BooleanValue(true)); 
  internet.SetRoutingHelper (blackhole_aodv);
  internet.Install (blackhole);

  
  // Set up Addresses
  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifcont = ipv4.Assign (devices);


  NS_LOG_INFO ("Create Applications.");

  // UDP connection from node 0 to node 3

  uint16_t sinkPort = 6;
  Address sinkAddress (InetSocketAddress (ifcont.GetAddress (3), sinkPort)); // interface of n3
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (c.Get (3)); //n3 as sink
  sinkApps.Start (Seconds (0.));
  sinkApps.Stop (Seconds (100.));

  Ptr<Socket> ns3UdpSocket = Socket::CreateSocket (c.Get (0), UdpSocketFactory::GetTypeId ()); //source at n0

  // Create UDP application at node 0
  Ptr<NormalApp> app = CreateObject<NormalApp> ();
              //udp    interface to node 3, packet size, number of packets, speed
  app->Setup (ns3UdpSocket, sinkAddress, 1040, 20, DataRate ("250Kbps"));
  //add application to node 0 which establishes connection and sends 20 UDP packets
  c.Get (0)->AddApplication (app);

  app->SetStartTime (Seconds (40.));
  app->SetStopTime (Seconds (100.));

// Set Mobility for all nodes

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject <ListPositionAllocator>();
  positionAlloc ->Add(Vector(100, 600, 0)); // node0
  positionAlloc ->Add(Vector(300, 600, 0)); // node1 
  positionAlloc ->Add(Vector(500, 600, 0)); // node2
  positionAlloc ->Add(Vector(700, 600, 0)); // node3
  positionAlloc ->Add(Vector(300, 400, 0)); // node4 
  positionAlloc ->Add(Vector(300, 200, 0)); // node5
  positionAlloc ->Add(Vector(300, 0, 0)); // node6

  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(c);

// ANIMATION SETTINGS ------------------------------------------------

  AnimationInterface anim ("Sim2Animation.xml"); // Mandatory
  AnimationInterface::SetConstantPosition (c.Get (0), 100, 580);
  AnimationInterface::SetConstantPosition (c.Get (1), 300, 580);
  AnimationInterface::SetConstantPosition (c.Get (2), 500, 580);
  AnimationInterface::SetConstantPosition (c.Get (3), 700, 580); 
  AnimationInterface::SetConstantPosition (c.Get (4), 300, 400);
  AnimationInterface::SetConstantPosition (c.Get (5), 300, 200);
  AnimationInterface::SetConstantPosition (c.Get (6), 300, 0); 
  anim.EnablePacketMetadata(true);

  //set nonmalicious node green and increase size
  NodeContainer::Iterator i;
  for (i = not_malicious.Begin (); i != not_malicious.End (); ++i)
  {
    anim.UpdateNodeColor(*i, 0, 255 , 0);
    anim.UpdateNodeSize((*i)->GetId(), 19.0, 19.0);
  }

    //increase size of black hole and turn it black
  anim.UpdateNodeSize(blackhole.Get(0)->GetId(), 19.0, 19.0);
  anim.UpdateNodeColor(blackhole.Get(0), 0, 0 , 0);

  
  //setup route output
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("sim2.routes", std::ios::out);
  aodv.PrintRoutingTableAllAt (Seconds (45), routingStream);
  // Trace Received Packets
  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback (&ReceivePacket));


//
// Calculate Throughput using Flowmonitor
//
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();


//
// Now, do the actual simulation.
//
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds(100.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if ((t.sourceAddress=="10.1.2.1" && t.destinationAddress == "10.1.2.4"))
      {
          std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
          std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
          std::cout << "  Delivery %:   " << (float)i->second.rxBytes / (float)i->second.txBytes * 100 << "%\n";
      	  std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024  << " Mbps\n";
      }
     }

  monitor->SerializeToXmlFile("simulation2.flowmon", true, true);


}
