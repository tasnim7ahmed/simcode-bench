#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/udp-echo-helper.h"

int main(int argc, char *argv[])
{
    // 1. Command Line Parsing (optional but good practice)
    ns3::CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // 2. Configure LTE and EPC helpers
    ns3::Ptr<ns3::LteHelper> lteHelper = ns3::CreateObject<ns3::LteHelper>();
    ns3::Ptr<ns3::EpcHelper> epcHelper = ns3::CreateObject<ns3::EpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Set the UE IP address pool to 10.1.1.0/24
    epcHelper->SetUePool(ns3::Ipv4Address("10.1.1.0"), ns3::Ipv4Mask("255.255.255.0"));

    // 3. Create Nodes: one eNodeB and one UE
    ns3::NodeContainer enbNodes;
    enbNodes.Create(1);
    ns3::NodeContainer ueNodes;
    ueNodes.Create(1);

    // 4. Install Internet Stack on eNodeB and UE nodes
    ns3::Ptr<ns3::InternetStackHelper> internetStackHelper = ns3::CreateObject<ns3::InternetStackHelper>();
    internetStackHelper->Install(enbNodes);
    internetStackHelper->Install(ueNodes);

    // 5. Install LTE devices on nodes
    ns3::NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    ns3::NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // 6. Attach UE to eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // 7. Configure Mobility for UE
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", ns3::RectangleValue(ns3::Rectangle(-50, 50, -50, 50)));
    mobility.Install(ueNodes);

    // Configure Mobility for eNodeB (static)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);

    // Populate routing tables (needed for global routing, especially involving EPC)
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 8. Configure Applications
    // Get UE's assigned IPv4 address (it will be assigned by EPC)
    // The UE's LTE interface is typically at index 1 (index 0 is localhost)
    ns3::Ptr<ns3::Ipv4> ueIpv4 = ueNodes.Get(0)->GetObject<ns3::Ipv4>();
    ns3::Ipv4Address ueAddress = ueIpv4->GetAddress(1, 0).GetLocal(); // Get address from interface 1 (LTE)

    // UDP Echo Server on UE
    uint16_t echoPort = 9;
    ns3::UdpEchoServerHelper echoServer(echoPort);
    ns3::ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(0));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(9.0));

    // UDP Echo Client on eNodeB
    ns3::UdpEchoClientHelper echoClient(ueAddress, echoPort);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(10));
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(512));
    ns3::ApplicationContainer clientApps = echoClient.Install(enbNodes.Get(0));
    clientApps.Start(ns3::Seconds(1.0));
    clientApps.Stop(ns3::Seconds(9.0));

    // 9. Set Simulation Stop Time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // 10. Run Simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}