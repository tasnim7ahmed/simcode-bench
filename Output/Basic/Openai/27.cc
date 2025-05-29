#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <iomanip>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMcsGoodputSim");

struct SimParams
{
  double simTime;            // seconds
  bool useUdp;               // true = UDP, false = TCP
  bool rtsCts;               // true = enable RTS/CTS
  double distance;           // meters
  double expThroughput;      // Mbit/s, for use if needed
};

void PrintResults (std::string mode, uint32_t channelWidth, bool shortGi, int mcs,
                   double throughput, double offered, std::string trafficType)
{
  std::cout << std::fixed << std::setprecision(2)
            << "[RESULT] Mode=" << mode 
            << "  Width=" << channelWidth << "MHz"
            << "  SGI=" << (shortGi ? "Yes" : "No")
            << "  MCS=" << mcs
            << "  Traffic=" << trafficType
            << "  Offered=" << offered << " Mbit/s"
            << "  Goodput=" << throughput << " Mbit/s"
            << std::endl;
}

int main (int argc, char *argv[])
{
  SimParams params;
  params.simTime = 10.0;
  params.useUdp = true;
  params.rtsCts = false;
  params.distance = 5.0;
  params.expThroughput = 200.0;

  CommandLine cmd;
  cmd.AddValue ("simTime", "Simulation time in seconds", params.simTime);
  cmd.AddValue ("udp", "Use UDP if true, TCP if false", params.useUdp);
  cmd.AddValue ("rtsCts", "Enable RTS/CTS", params.rtsCts);
  cmd.AddValue ("distance", "Distance between STA and AP (meters)", params.distance);
  cmd.AddValue ("expectedThroughput", "Expected offered throughput (Mbit/s)", params.expThroughput);
  cmd.Parse (argc, argv);

  std::string phyMode ("HtMcs0");

  std::vector<uint32_t> mcsVals = {0,1,2,3,4,5,6,7};
  std::vector<uint32_t> channelWidths = {20, 40};
  std::vector<bool> shortGis = {false, true};

  uint64_t payloadSize = 1472;
  uint32_t port = 9;

  for (auto mcs : mcsVals)
  {
    for (auto channelWidth : channelWidths)
    {
      for (auto shortGi : shortGis)
      {
        NodeContainer wifiStaNode, wifiApNode;
        wifiStaNode.Create (1);
        wifiApNode.Create (1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
        phy.SetChannel (channel.Create ());
        phy.Set ("ChannelWidth", UintegerValue (channelWidth));
        phy.Set ("ShortGuardEnabled", BooleanValue (shortGi));

        WifiHelper wifi;
        wifi.SetStandard (WIFI_STANDARD_80211n_5GHZ);

        std::ostringstream oss;
        oss << "HtMcs" << mcs;
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue (oss.str ()),
                                      "ControlMode", StringValue (oss.str ()),
                                      "RtsCtsThreshold", UintegerValue (params.rtsCts? 1 : 2347));

        WifiMacHelper mac;
        Ssid ssid = Ssid ("ns3-80211n");
        mac.SetType ("ns3::StaWifiMac",
                     "Ssid", SsidValue (ssid),
                     "ActiveProbing", BooleanValue (false));
        NetDeviceContainer staDevice = wifi.Install (phy, mac, wifiStaNode);

        mac.SetType ("ns3::ApWifiMac",
                     "Ssid", SsidValue (ssid));
        NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
        positionAlloc->Add (Vector (0.0, 0.0, 0.0));
        positionAlloc->Add (Vector (params.distance, 0.0, 0.0));
        mobility.SetPositionAllocator (positionAlloc);
        mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        mobility.Install (wifiApNode);
        mobility.Install (wifiStaNode);

        InternetStackHelper stack;
        stack.Install (wifiApNode);
        stack.Install (wifiStaNode);

        Ipv4AddressHelper address;
        address.SetBase ("10.1.3.0", "255.255.255.0");
        Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
        Ipv4InterfaceContainer staInterface = address.Assign (staDevice);

        ApplicationContainer serverApp, clientApp;
        double offeredMbps = params.expThroughput;

        if (params.useUdp)
        {
          UdpServerHelper server (port);
          serverApp = server.Install (wifiStaNode.Get (0));
          serverApp.Start (Seconds (0.0));
          serverApp.Stop (Seconds (params.simTime + 1));

          uint32_t pktSize = payloadSize;
          double offeredBps = offeredMbps * 1e6 / 8.0;
          uint32_t rateInt = static_cast<uint32_t>(offeredBps);
          std::ostringstream ossRate;
          ossRate << static_cast<uint64_t>(offeredMbps) << "Mbps";
          UdpClientHelper client (staInterface.GetAddress (0), port);
          client.SetAttribute ("MaxPackets", UintegerValue (0xFFFFFFFF));
          client.SetAttribute ("Interval", TimeValue (Seconds (pktSize * 8.0 / offeredBps)));
          client.SetAttribute ("PacketSize", UintegerValue (pktSize));
          clientApp = client.Install (wifiApNode.Get (0));
          clientApp.Start (Seconds (1.0));
          clientApp.Stop (Seconds (params.simTime + 1));
        }
        else
        {
          Address apLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
          PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", apLocalAddress);
          serverApp = sinkHelper.Install (wifiStaNode.Get (0));
          serverApp.Start (Seconds (0.0));
          serverApp.Stop (Seconds (params.simTime + 1));

          OnOffHelper client ("ns3::TcpSocketFactory", InetSocketAddress (staInterface.GetAddress (0), port));
          std::ostringstream ossT;
          ossT << static_cast<uint64_t>(offeredMbps) << "Mbps";
          client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
          client.SetAttribute ("DataRate", StringValue (ossT.str ()));
          client.SetAttribute ("MaxBytes", UintegerValue (0));
          clientApp = client.Install (wifiApNode.Get (0));
          clientApp.Start (Seconds (1.0));
          clientApp.Stop (Seconds (params.simTime + 1));
        }

        FlowMonitorHelper flowmon;
        Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

        Simulator::Stop (Seconds (params.simTime + 1));
        Simulator::Run ();

        monitor->CheckForLostPackets ();
        FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
        double rxBytes = 0.0, txBytes = 0.0;
        for (const auto& kv : stats)
        {
          FlowMonitor::FlowStats s = kv.second;
          rxBytes += s.rxBytes;
          txBytes += s.txBytes;
        }

        double throughput = (rxBytes * 8.0) / (1e6 * params.simTime); // Mbit/s
        offeredMbps = (txBytes * 8.0) / (1e6 * params.simTime);

        std::string trafficType = params.useUdp ? "UDP" : "TCP";
        PrintResults ("802.11n", channelWidth, shortGi, mcs,
                      throughput, offeredMbps, trafficType);

        Simulator::Destroy ();
      }
    }
  }

  return 0;
}