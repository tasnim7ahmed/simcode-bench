#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiTwoStaApUdpExample");

struct StaStats
{
  uint32_t packetsTx = 0;
  uint32_t bytesRx = 0;
  Time firstPacketRx;
  Time lastPacketRx;
};

std::map<uint32_t, StaStats> g_staStats;

void
TxAppTrace (Ptr<const Packet> packet, const Address & addr, uint32_t staId)
{
  g_staStats[staId].packetsTx++;
}

void
RxAppTrace (Ptr<const Packet> packet, const Address & addr, uint32_t staId)
{
  uint32_t pktSize = packet->GetSize ();
  if (g_staStats[staId].bytesRx == 0)
    g_staStats[staId].firstPacketRx = Simulator::Now ();
  g_staStats[staId].bytesRx += pktSize;
  g_staStats[staId].lastPacketRx = Simulator::Now ();
}

int main (int argc, char *argv[])
{
  uint32_t nSta = 2;
  double simulationTime = 10.0; // seconds

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nSta);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ac);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");

  // Stations
  NetDeviceContainer staDevices;
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  // Access Point
  NetDeviceContainer apDevice;
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (5.0),
                                "DeltaY", DoubleValue (0.0),
                                "GridWidth", UintegerValue (3),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);

  // UDP server on AP
  uint16_t port = 9000;
  ApplicationContainer serverApps;
  UdpServerHelper serverHelper (port);
  serverApps = serverHelper.Install (wifiApNode.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simulationTime + 1));

  // UDP client apps on each STA
  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < nSta; ++i)
    {
      UdpClientHelper client (apInterface.GetAddress (0), port);
      client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
      client.SetAttribute ("Interval", TimeValue (MilliSeconds (20))); // ~50 pkt/s
      client.SetAttribute ("PacketSize", UintegerValue (1024));
      ApplicationContainer app = client.Install (wifiStaNodes.Get (i));
      app.Start (Seconds (1.0 + i * 0.1));
      app.Stop (Seconds (simulationTime + 1));
      // Attach packet TX trace for this STA
      Ptr<Application> udpApp = app.Get (0);
      udpApp->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxAppTrace, i));
      // Attach receive trace to the server (for RX statistics)
      Ptr<Node> apNode = wifiApNode.Get (0);
      Ptr<Application> serverApp = serverApps.Get (0);
      serverApp->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&RxAppTrace, i));
    }

  phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcap ("wifi-ap", apDevice.Get (0), true);
  for (uint32_t i = 0; i < nSta; ++i)
    {
      phy.EnablePcap ("wifi-sta", staDevices.Get (i), true);
    }

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();

  // Print per-sta stats
  std::cout << "STA Results:" << std::endl;
  for (uint32_t i = 0; i < nSta; ++i)
    {
      StaStats &stats = g_staStats[i];
      double throughput = 0.0;
      if (stats.lastPacketRx > stats.firstPacketRx)
        {
          throughput = (stats.bytesRx * 8.0) / (stats.lastPacketRx - stats.firstPacketRx).GetSeconds () / 1e6; // Mbps
        }
      std::cout << "STA " << i+1 << ": " << std::endl;
      std::cout << "  Packets sent: " << stats.packetsTx << std::endl;
      std::cout << "  Bytes received at AP: " << stats.bytesRx << std::endl;
      std::cout << "  Throughput at AP: " << throughput << " Mbps" << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}