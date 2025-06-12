#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteTcpExample");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("LteTcpExample", LOG_LEVEL_INFO);

    // Create nodes: two eNodeBs (base stations) and one UE (user equipment)
    NodeContainer eNodeBs, ueNodes;
    eNodeBs.Create(2);
    ueNodes.Create(1);

    // Create EPC helper and LTE helper
    Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Install LTE stack
    lteHelper->Install(eNodeBs);
    lteHelper->Install(ueNodes);

    // Set up mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(eNodeBs);

    // Set mobility for UE to move between eNodeBs
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel");
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(50.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(ueNodes);

    // Install Internet stack on UE
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses to the LTE interfaces
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(DevicesContainer(ueNodes));

    // Install applications on UE and eNodeB
    uint16_t port = 8080;
    ApplicationContainer serverApp;
    UdpServerHelper udpServer(port);
    serverApp = udpServer.Install(eNodeBs.Get(0)); // eNodeB 1 as server
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper udpClient(ueIpIface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp = udpClient.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Handover between two eNodeBs during the simulation
    lteHelper->ActivateHandover(ueNodes.Get(0), eNodeBs.Get(0), eNodeBs.Get(1));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
