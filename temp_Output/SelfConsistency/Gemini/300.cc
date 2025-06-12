#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/propagation-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/stats-module.h"
#include "ns3/wifi-module.h"
#include "ns3/zigbee-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ZigbeeWsnExample");

int main(int argc, char* argv[]) {
  bool enablePcap = true;

  CommandLine cmd;
  cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.Parse(argc, argv);

  // Configure logging
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  LogComponentEnable("ZigbeeMac", LOG_LEVEL_ALL);
  LogComponentEnable("ZigbeePhy", LOG_LEVEL_ALL);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(10);

  // Configure ZigBee devices
  ZigbeeHelper zigbeeHelper;
  NetDeviceContainer devices = zigbeeHelper.Install(nodes);

  // Set channel number (11 to 26)
  zigbeeHelper.SetChannelNumber(11);

  // Set transmission mode (PRO or not PRO)
  zigbeeHelper.SetPromiscuous(false);

  // Install the internet stack on the nodes
  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Set the coordinator
  Ptr<ZigbeeNetDevice> coordinator = devices.Get(0)->GetObject<ZigbeeNetDevice>();
  coordinator->SetAddress(Mac16Address("00:01"));
  zigbeeHelper.AssociateToPan(devices.Get(0), 0x1234);

  // Set the end devices
  for (uint32_t i = 1; i < nodes.GetN(); ++i) {
    Ptr<ZigbeeNetDevice> endDevice = devices.Get(i)->GetObject<ZigbeeNetDevice>();
    endDevice->SetAddress(Mac16Address::Allocate());
    zigbeeHelper.AssociateToPan(devices.Get(i), 0x1234);
  }

  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", StringValue("0|100|0|100"),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
  mobility.Install(nodes);

  // Create OnOff application to send UDP datagrams from end devices to coordinator
  uint16_t port = 9;  // Discard port (RFC 863)

  for (uint32_t i = 1; i < nodes.GetN(); ++i) {
    OnOffHelper onOffHelper("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(0), port)));
    onOffHelper.SetConstantRate(DataRate("256kbps")); // Sending rate.  Irrelevant, rate is controlled by the period
    onOffHelper.SetAttribute("PacketSize", UintegerValue(128)); // bytes
    ApplicationContainer clientApps = onOffHelper.Install(nodes.Get(i));
    clientApps.Start(Seconds(2.0 + i * 0.1)); // stagger start times
    clientApps.Stop(Seconds(10.0));
  }

  // Create a server application on the coordinator to receive UDP datagrams
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // Enable PCAP tracing
  if (enablePcap) {
    zigbeeHelper.EnablePcapAll("zigbee-wsn");
  }

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}