#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    // 1. Create Nodes
    ns3::NodeContainer gNbNodes;
    gNbNodes.Create(1);
    ns3::NodeContainer ueNodes;
    ueNodes.Create(2);

    // 2. Configure Mobility
    ns3::MobilityHelper mobility;

    // gNB: Stationary at (50, 50, 0)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gNbNodes);
    gNbNodes.Get(0)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(50.0, 50.0, 0.0));

    // UEs: Random Walk Mobility in a 100x100 meter area
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", ns3::StringValue("0|100|0|100"),
                              "Speed", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1.0]"), // 1 m/s
                              "Direction", ns3::StringValue("ns3::UniformRandomVariable[Min=0.0|Max=6.283185307]")); // Random initial direction (0 to 2*PI radians)
    mobility.Install(ueNodes);

    // 3. Install NR (5G) Net Devices
    ns3::nr::NrHelper nrHelper;
    nrHelper.SetStandard(ns3::nr::NsaNrHelper::STANDARD_NR_FR2); // Use FR2 for mmWave characteristics

    ns3::NetDeviceContainer gNbDevs = nrHelper.InstallGnbDevice(gNbNodes);
    ns3::NetDeviceContainer ueDevs = nrHelper.InstallUeDevice(ueNodes);

    // 4. Install Internet Stack
    ns3::InternetStackHelper internet;
    internet.Install(gNbNodes);
    internet.Install(ueNodes);

    // 5. Assign IPv4 Addresses (192.168.1.0/24 subnet)
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");

    ns3::Ipv4InterfaceContainer gNbIpIfs = ipv4.Assign(gNbDevs);
    ns3::Ipv4InterfaceContainer ueIpIfs = ipv4.Assign(ueDevs);

    // 6. Attach UEs to gNB
    nrHelper.AttachToClosestGnb(ueDevs, gNbDevs);

    // 7. Configure UDP Echo Server on UE2 (ueNodes.Get(1))
    uint16_t port = 9; // Standard Echo Protocol port
    ns3::UdpEchoServerHelper echoServer(port);
    ns3::ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(1));
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // 8. Configure UDP Echo Client on UE1 (ueNodes.Get(0))
    // Target IP is the IP address of UE2 (the server)
    ns3::Ipv4Address ue2IpAddress = ueIpIfs.GetAddress(1);
    ns3::UdpEchoClientHelper echoClient(ue2IpAddress, port);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(5));
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024));

    ns3::ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
    clientApps.Start(ns3::Seconds(1.0)); // Client starts after server is up
    clientApps.Stop(ns3::Seconds(10.0));

    // 9. Simulation Duration
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // 10. Run Simulation
    ns3::Simulator::Run();

    // 11. Cleanup
    ns3::Simulator::Destroy();

    return 0;
}