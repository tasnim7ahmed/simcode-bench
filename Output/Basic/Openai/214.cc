#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiFourNodeApStaExample");

void
TraceSentPacket (Ptr<const Packet> packet)
{
  std::cout << Simulator::Now ().GetSeconds () << "s Packet sent, size = " << packet->GetSize () << " bytes" << std::endl;
}

void
TraceRecvPacket (Ptr<const Packet> packet, const Address &)
{
  std::cout << Simulator::Now ().GetSeconds () << "s Packet received, size = " << packet->GetSize () << " bytes" << std::endl;
}

int
main (int argc, char *argv[])
{
  double simulationTime = 10.0;
  uint32_t payloadSize = 1024;
  std::string dataRate = "10Mbps";

  // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (3);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ac);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");

  // Station MAC
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  // AP MAC
  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX", DoubleValue (0.0),
    "MinY", DoubleValue (0.0),
    "DeltaX", DoubleValue (5.0),
    "DeltaY", DoubleValue (10.0),
    "GridWidth", UintegerValue (2),
    "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);

  // Install UdpServer on STA 2
  uint16_t serverPort = 4000;
  UdpServerHelper serverHelper (serverPort);
  ApplicationContainer serverApp = serverHelper.Install (wifiStaNodes.Get (2));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (simulationTime));

  // UdpClients on STA 0 and STA 1 targeting STA 2
  UdpClientHelper clientHelper (staInterfaces.GetAddress (2), serverPort);
  clientHelper.SetAttribute ("MaxPackets", UintegerValue (1000000));
  clientHelper.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  clientHelper.SetAttribute ("DataRate", StringValue (dataRate));

  ApplicationContainer clientApps;
  clientApps.Add (clientHelper.Install (wifiStaNodes.Get (0)));
  clientApps.Add (clientHelper.Install (wifiStaNodes.Get (1)));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  // Trace packet transmit and receive at STA 2
  Ptr<NetDevice> sta2Device = staDevices.Get (2);
  sta2Device->TraceConnectWithoutContext ("PhyRxEnd",
    MakeCallback ([] (Ptr<const Packet> pkt) { TraceRecvPacket (pkt, Address ()); }));
  staDevices.Get (0)->TraceConnectWithoutContext ("PhyTxEnd", MakeCallback (&TraceSentPacket));
  staDevices.Get (1)->TraceConnectWithoutContext ("PhyTxEnd", MakeCallback (&TraceSentPacket));

  // Enable pcap tracing
  phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcap ("wifi-ap", apDevice, true);
  phy.EnablePcap ("wifi-sta-0", staDevices.Get (0), true);
  phy.EnablePcap ("wifi-sta-1", staDevices.Get (1), true);
  phy.EnablePcap ("wifi-sta-2", staDevices.Get (2), true);

  // FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();

  // FlowMonitor results
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  double totalBytesRx = 0;
  uint32_t totalPacketsRx = 0;
  uint32_t totalPacketsLost = 0;

  for (auto const &flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      if ((t.destinationAddress == staInterfaces.GetAddress (2)) && (t.protocol == 17))
        {
          totalBytesRx += flow.second.rxBytes;
          totalPacketsRx += flow.second.rxPackets;
          totalPacketsLost += flow.second.lostPackets;
        }
    }

  double throughput = (totalBytesRx * 8) / (simulationTime - 2.0) / 1e6; // Mbps, subtracting client start time
  std::cout << "=== Performance Results ===" << std::endl;
  std::cout << "Throughput to STA2: " << throughput << " Mbps" << std::endl;
  std::cout << "Total packets received: " << totalPacketsRx << std::endl;
  std::cout << "Total packets lost: " << totalPacketsLost << std::endl;

  Simulator::Destroy ();
  return 0;
}