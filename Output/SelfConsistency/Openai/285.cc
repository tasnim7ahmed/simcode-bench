/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * ns-3 LTE network: 1 eNodeB, 3 UEs, UDP client/server over LTE
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
  // Enable logging for useful components
  // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // Simulation parameters
  uint16_t numberOfUes = 3;
  uint16_t numberOfEnbs = 1;
  double simTime = 10.0;
  double areaSize = 100.0;
  uint16_t serverPort = 5000; // UDP server port

  // LTE/EPC components
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);

  // Create nodes: eNodeB and UEs
  NodeContainer enbNodes;
  NodeContainer ueNodes;
  enbNodes.Create(numberOfEnbs);
  ueNodes.Create(numberOfUes);

  // Install Mobility Model
  // eNodeB is stationary at the middle
  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
  enbPositionAlloc->Add(Vector(areaSize/2, areaSize/2, 0));
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbMobility.SetPositionAllocator(enbPositionAlloc);
  enbMobility.Install(enbNodes);

  // UEs: RandomWalk2d within 100x100 area
  MobilityHelper ueMobility;
  ueMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("Uniform(0.0, 100.0)"),
                                  "Y", StringValue("Uniform(0.0, 100.0)"));
  ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0.0, areaSize, 0.0, areaSize)));
  ueMobility.Install(ueNodes);

  // Install LTE Devices
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

  // Install the IP stack on UEs
  InternetStackHelper internet;
  internet.Install(ueNodes);

  // Assign IP address to UEs
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

  // Attach UE devices to eNodeB
  for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
      lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

  // (Optional) enable default routing for UEs
  for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
      Ptr<Node> ueNode = ueNodes.Get(i);
      Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(ueNode->GetObject<Ipv4>()->GetRoutingProtocol());
      ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

  // Install UDP server on UE 0
  UdpServerHelper udpServer(serverPort);
  ApplicationContainer serverApps = udpServer.Install(ueNodes.Get(0));
  serverApps.Start(Seconds(0.1));
  serverApps.Stop(Seconds(simTime));

  // Install UDP client on UE 1 (sending to UE 0)
  UdpClientHelper udpClient(ueIpIface.GetAddress(0), serverPort);
  udpClient.SetAttribute("MaxPackets", UintegerValue(1000000));
  udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10))); // 10 ms
  udpClient.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(1));
  clientApps.Start(Seconds(0.5));
  clientApps.Stop(Seconds(simTime));

  // Enable PCAP tracing
  lteHelper->EnableTraces();
  epcHelper->EnablePcap("lte-epc-enb", enbLteDevs.Get(0), true);
  epcHelper->EnablePcap("lte-epc-ue0", ueLteDevs.Get(0), true);
  epcHelper->EnablePcap("lte-epc-ue1", ueLteDevs.Get(1), true);
  epcHelper->EnablePcap("lte-epc-ue2", ueLteDevs.Get(2), true);

  // Run simulation
  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}