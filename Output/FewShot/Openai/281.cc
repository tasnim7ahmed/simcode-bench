#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/point-to-point-epc-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable relevant logging
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create nodes: one eNodeB, three UEs
    NodeContainer ueNodes;
    ueNodes.Create(3);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Create EPC helper and LTE helper
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create a remote host (to act as SGW/PGW gateway's peer)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Connect PGW to remoteHost
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetPgwNode(), remoteHost);

    // Install Internet stack on remote host (not used directly in this scenario)
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Assign IP addresses for the Internet
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIfaces = ipv4h.Assign(internetDevices);
    // Assign interface on the remote host
    Ipv4Address remoteHostAddr = internetIfaces.GetAddress(1);

    // Install Mobility Model for eNodeB (fixed)
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(enbNodes);
    enbNodes.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(25.0, 25.0, 0.0));

    // Install Mobility Model for UEs (RandomWalk2dMobilityModel)
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0.0, 50.0, 0.0, 50.0)),
                                "Distance", DoubleValue(5.0));
    ueMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    ueMobility.Install(ueNodes);

    // Install LTE devices to the nodes
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install IP stack on UEs
    internet.Install(ueNodes);

    // Assign IP addresses to UEs and attach them to eNodeB
    Ipv4InterfaceContainer ueIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueDevs.Get(i), enbDevs.Get(0));
    }

    // Install UDP server (on the eNodeB node itself, via its LTE IP)
    uint16_t udpPort = 9;

    // Get the eNodeB's SGi IP address for application use (assign a loopback address for demo)
    // In LTE EPC model, UE-to-ENB traffic is handled via IP addresses assigned to UEs,
    // but the ENB doesn't have a direct SGi IP for app traffic.
    // So set up the server on the remoteHost or on one of the LTE nodes. We'll use first UE's address as server.
    // To fulfill the request, install the UDP server on the eNodeB node, bind to 0.0.0.0.

    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(0.5));
    serverApps.Stop(Seconds(10.0));

    // Install UDP clients on each UE, targeting the eNodeB's address
    // For LTE, UEs send to eNodeB/PGW (downlink is the normal flow);
    // for demo, UEs will send to eNodeB's "SGi" interface which technically eNodeB doesn't have directly.
    // We'll bind UDPServer to eNodeB and direct clients to its IP on LTE subnet (for this scenario, use the LTE address).
    Ptr<Ipv4> enbIpv4 = enbNodes.Get(0)->GetObject<Ipv4>();
    Ipv4Address enbAddr = enbIpv4->GetAddress(1, 0).GetLocal();

    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        UdpClientHelper udpClient(enbAddr, udpPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(10));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(10.0));
    }

    // Enable LTE traces (optional, can be commented)
    // lteHelper->EnableTraces();

    Simulator::Stop(Seconds(10.01));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}