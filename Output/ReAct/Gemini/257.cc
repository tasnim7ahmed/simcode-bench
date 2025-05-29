#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/epc-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbNetDevice::DlEarfcn", UintegerValue (100));
  Config::SetDefault ("ns3::LteEnbNetDevice::UlEarfcn", UintegerValue (18100));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(2);

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
  Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);

  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbLteDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueLteDevs);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(enbNodes);
  mobility.Install(ueNodes);

  uint16_t dlPort = 9;
  UdpServerHelper dlPacketSinkHelper(dlPort);
  ApplicationContainer dlPacketSinkApp = dlPacketSinkHelper.Install(ueNodes.Get(1));
  dlPacketSinkApp.Start(Seconds(1.0));
  dlPacketSinkApp.Stop(Seconds(10.0));

  uint32_t packetSize = 512;
  uint32_t maxPackets = 2;
  Time interPacketInterval = Seconds(1);
  UdpClientHelper ulPacketSourceHelper(ueIpIface.GetAddress(1), dlPort);
  ulPacketSourceHelper.SetAttribute("MaxPackets", UintegerValue(maxPackets));
  ulPacketSourceHelper.SetAttribute("Interval", TimeValue(interPacketInterval));
  ulPacketSourceHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer ulPacketSourceApp = ulPacketSourceHelper.Install(ueNodes.Get(0));
  ulPacketSourceApp.Start(Seconds(2.0));
  ulPacketSourceApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}