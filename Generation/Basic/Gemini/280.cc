#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    // 1. Configure default TCP attributes
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
    Config::SetDefault("ns3::DropTailQueue::MaxBytes", StringValue("100000000")); // Large buffer

    // 2. Create Nodes
    NodeContainer gnbNode;
    gnbNode.Create(1); // One gNodeB
    NodeContainer ueNodes;
    ueNodes.Create(2); // Two UEs

    // 3. Setup EPC (Evolved Packet Core) Helper
    Ptr<nr::NrPointToPointEpcHelper> epcHelper = CreateObject<nr::NrPointToPointEpcHelper>();

    // 4. Setup NR Helper and connect to EPC
    Ptr<nr::NrHelper> nrHelper = CreateObject<nr::NrHelper>();
    nrHelper->SetEpcHelper(epcHelper);

    // Configure NR settings: Frequency Range 1 (FR1), 3.5 GHz band, 30 MHz bandwidth, Numerology 1 (30 kHz SCS)
    nrHelper->SetGnbDeviceAttribute("BandwidthPartMap", StringValue("ns3::LteFr1BandwidthPartMapParameters[CenterFrequency=3500000000|Bandwidth=30000000|Numerology=1]"));
    nrHelper->SetUeDeviceAttribute("BandwidthPartMap", StringValue("ns3::LteFr1BandwidthPartMapParameters[CenterFrequency=3500000000|Bandwidth=30000000|Numerology=1]"));

    // 5. Install Internet Stack on all nodes (gNB, UEs, and PGW from EPC)
    InternetStackHelper internet;
    internet.Install(gnbNode);
    internet.Install(ueNodes);
    internet.Install(epcHelper->GetPgwNode()); // Install on the PGW node created by EPC helper

    // 6. Install NR Devices on gNB and UEs
    NetDeviceContainer gnbNrDevs = nrHelper->InstallGnbDevice(gnbNode.Get(0), epcHelper);
    NetDeviceContainer ueNrDevs = nrHelper->InstallUeDevice(ueNodes, epcHelper);

    // 7. Assign IP Addresses to NR interfaces
    Ipv4InterfaceContainer gnbIpIfaces = epcHelper->AssignGnbIpv4Address(gnbNrDevs);
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(ueNrDevs);

    // 8. Configure Mobility
    MobilityHelper mobility;

    // gNodeB at a fixed position (center of 50x50 area)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNode);
    gnbNode.Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(25.0, 25.0, 0.0));

    // UEs with Random Walk Mobility Model within 50x50 area
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)), // 50x50 meter area
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]")); // 1 m/s speed
    mobility.Install(ueNodes);

    // 9. Setup TCP Applications
    // Server (PacketSink) on UE2 (ueNodes.Get(1))
    uint16_t port = 8080;
    Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer serverApps = sinkHelper.Install(ueNodes.Get(1)); // UE2 is index 1
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Client (OnOffApplication) on UE1 (ueNodes.Get(0))
    Ptr<Node> clientNode = ueNodes.Get(0);
    Ipv4Address serverIp = ueIpIfaces.GetAddress(1); // Get IP of UE2 (server)

    OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(serverIp, port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]")); // Client always sends
    onoff.SetAttribute("PacketSize", UintegerValue(1024)); // 1 KB packets
    onoff.SetAttribute("DataRate", StringValue("5Mbps")); // 5 Mbps data rate

    ApplicationContainer clientApps = onoff.Install(clientNode);
    clientApps.Start(Seconds(1.0)); // Start client after 1 second
    clientApps.Stop(Seconds(10.0));

    // 10. Simulation Start and End
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}