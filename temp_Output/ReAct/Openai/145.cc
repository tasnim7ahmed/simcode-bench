#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/applications-module.h"
#include "ns3/pcap-file-wrapper.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshExample");

class UdpStatistics 
{
public:
  UdpStatistics() : txPackets(0), rxPackets(0) {}

  void TxCallback (Ptr<const Packet> p)
  {
    txPackets++;
  }

  void RxCallback (Ptr<const Packet> p, const Address &address)
  {
    rxPackets++;
  }

  uint32_t txPackets;
  uint32_t rxPackets;
};

int main(int argc, char *argv[])
{
  // Set up logging for debugging (optional)
  // LogComponentEnable("MeshExample", LOG_LEVEL_INFO);

  // Step 1: Create nodes
  NodeContainer nodes;
  nodes.Create(3);

  // Step 2: Configure Wi-Fi mesh
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  MeshHelper mesh = MeshHelper::Default ();
  mesh.SetStackInstaller ("ns3::Dot11sStack");
  mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
  mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));
  NetDeviceContainer meshDevices = mesh.Install (wifiPhy, nodes);

  // Step 3: Mobility - static positions
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));
  positionAlloc->Add(Vector(5.0, 0.0, 0.0));
  positionAlloc->Add(Vector(2.5, 4.3, 0.0));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Step 4: Internet stack
  InternetStackHelper internetStack;
  internetStack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

  // Step 5: UDP traffic generation - node 0 sends to node 2
  uint16_t sinkPort = 5000;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(2), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  // UdpClient
  UdpClientHelper udpClient(sinkAddress, sinkPort);
  udpClient.SetAttribute("MaxPackets", UintegerValue(320));
  udpClient.SetAttribute("Interval", TimeValue(Seconds(0.03)));
  udpClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(10.0));

  // Also generate traffic from node 1 to node 0
  uint16_t sinkPortN0 = 5001;
  Address sinkAddressN0(InetSocketAddress(interfaces.GetAddress(0), sinkPortN0));
  PacketSinkHelper packetSinkHelperN0("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPortN0));
  ApplicationContainer sinkAppN0 = packetSinkHelperN0.Install(nodes.Get(0));
  sinkAppN0.Start(Seconds(0.0));
  sinkAppN0.Stop(Seconds(10.0));

  UdpClientHelper udpClient2(sinkAddressN0, sinkPortN0);
  udpClient2.SetAttribute("MaxPackets", UintegerValue(320));
  udpClient2.SetAttribute("Interval", TimeValue(Seconds(0.03)));
  udpClient2.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp2 = udpClient2.Install(nodes.Get(1));
  clientApp2.Start(Seconds(2.0));
  clientApp2.Stop(Seconds(10.0));

  // Step 6: PCAP Tracing
  wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  mesh.EnablePcapAll("mesh-pcap");

  // Step 7: UDP statistics callbacks
  UdpStatistics stats0, stats1;
  // Connect transmit callback for both clients
  Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::UdpClient/Tx", MakeCallback(&UdpStatistics::TxCallback, &stats0));
  Config::ConnectWithoutContext("/NodeList/1/ApplicationList/0/$ns3::UdpClient/Tx", MakeCallback(&UdpStatistics::TxCallback, &stats1));
  // Connect packet sink reception callbacks
  Ptr<PacketSink> sink2 = DynamicCast<PacketSink>(sinkApp.Get(0));
  Ptr<PacketSink> sink0 = DynamicCast<PacketSink>(sinkAppN0.Get(0));

  sink2->TraceConnectWithoutContext("Rx", MakeCallback(&UdpStatistics::RxCallback, &stats0));
  sink0->TraceConnectWithoutContext("Rx", MakeCallback(&UdpStatistics::RxCallback, &stats1));

  // Step 8: Run simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Output basic statistics
  std::cout << "Statistics for traffic Node0->Node2:" << std::endl;
  std::cout << "  Packets transmitted: " << stats0.txPackets << std::endl;
  std::cout << "  Packets received:    " << stats0.rxPackets << std::endl;

  std::cout << "Statistics for traffic Node1->Node0:" << std::endl;
  std::cout << "  Packets transmitted: " << stats1.txPackets << std::endl;
  std::cout << "  Packets received:    " << stats1.rxPackets << std::endl;

  Simulator::Destroy();
  return 0;
}