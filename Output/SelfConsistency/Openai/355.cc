#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-echo-helper.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
  // Enable logging (optional, comment out if not needed)
  // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer wifiNodes;
  wifiNodes.Create(3);

  // WiFi 802.11n (2.4GHz) setup
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns3-80211n-ssid");
  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes);

  // Set one of the nodes as AP (to enable communication)
  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(0)); // Node 0 as AP

  // Merge all devices into a single container (for IP assignment)
  NetDeviceContainer allDevices;
  for (uint32_t i = 0; i < wifiNodes.GetN(); ++i)
    {
      allDevices.Add(wifiNodes.Get(i)->GetDevice(0));
    }

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(0.0),
                               "MinY", DoubleValue(0.0),
                               "DeltaX", DoubleValue(5.0),
                               "DeltaY", DoubleValue(5.0),
                               "GridWidth", UintegerValue(2),
                               "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiNodes);

  // Internet stack and IP assignment
  InternetStackHelper stack;
  stack.Install(wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(allDevices);

  // UDP Echo Server on node 0 (AP)
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(wifiNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // UDP Echo Client on node 2
  UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9); // Node 0's IP
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(wifiNodes.Get(2));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}