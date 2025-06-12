#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/config-store.h"
#include "ns3/nr-module.h"
#include "ns3/nr-helper.h"
#include "ns3/nr-point-to-point-epc-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Simulation parameters
    double simTime = 10.0; // seconds
    uint32_t packetSize = 512; // bytes
    double interPacketInterval = 0.5; // seconds

    // Create core network (EPC) helper and NR Helper
    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetEpcHelper(epcHelper);

    // Set up 3GPP channel and error model
    nrHelper->SetGnbDeviceAttribute("UlBandwidth", UintegerValue(100));
    nrHelper->SetGnbDeviceAttribute("DlBandwidth", UintegerValue(100));
    nrHelper->SetGnbAttribute("TransmissionMode", UintegerValue(1));
    nrHelper->SetAttribute("BeamformingMethod", StringValue("CellScan"));

    // Create nodes: one gNB, one UE
    NodeContainer gnbNodes;
    gnbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Install mobility: gNB is stationary, UE is mobile
    MobilityHelper gnbMobility;
    gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gnbMobility.Install(gnbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    ueMobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(10.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(0.0),
        "DeltaY", DoubleValue(0.0),
        "GridWidth", UintegerValue(1),
        "LayoutType", StringValue("RowFirst"));
    ueMobility.Install(ueNodes);

    // Set UE initial velocity (moving in x direction)
    Ptr<MobilityModel> ueMobilityModel = ueNodes.Get(0)->GetObject<MobilityModel>();
    ueNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(2.0, 0.0, 0.0)); // 2 m/s along x

    // Install NR devices to the nodes
    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gnbNodes);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes);

    // Assign NR cells: one gNB (cellId 1), attach UE
    nrHelper->AssignCellIds();    
    nrHelper->AttachToClosestEnb(ueDevs, gnbDevs);

    // Configure the remote host as the destination for EPC
    Ptr<Node> remoteHost;
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    remoteHost = remoteHostContainer.Get(0);

    // Create internet stack for remote host
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Set up point-to-point link between EPC and remote host
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.001)));
    NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetPgwNode(), remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    // Install the IP stack on the UE
    internet.Install(ueNodes);

    // Assign IP address to UE device
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set default route for UE
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = 
            Ipv4RoutingHelper::GetRouting < Ipv4StaticRouting > (ueNodes.Get(u)->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Install IP stack on gNB for packet sink; assign IP
    internet.Install(gnbNodes);
    Ipv4AddressHelper ipv4g;
    ipv4g.SetBase("10.1.0.0", "255.255.255.0");
    NetDeviceContainer gnbEthDevs;
    NetDeviceContainer gnbEth = p2ph.Install(remoteHost, gnbNodes.Get(0));
    Ipv4InterfaceContainer gnbIpIfaces = ipv4g.Assign(gnbEth);

    // UDP Server application on the gNB
    uint16_t udpPort = 50000;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApp = udpServer.Install(gnbNodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // UDP Client application on the UE
    UdpClientHelper udpClient(gnbIpIfaces.GetAddress(1), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(uint32_t(simTime / interPacketInterval)));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = udpClient.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(1.0)); // Start after network is ready
    clientApp.Stop(Seconds(simTime));

    // Enable packet capture on the gNB device
    nrHelper->EnableTraces();

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}