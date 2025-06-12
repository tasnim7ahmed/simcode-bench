#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // LTE Helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Nodes: eNodeB and UEs
    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(1);
    ueNodes.Create(3);

    // Mobility model
    MobilityHelper mobility;

    // eNodeB position (fixed)
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(25.0, 25.0, 0.0));
    mobility.SetPositionAllocator(enbPositionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);

    // UEs with RandomWalk2dMobilityModel in 50x50 area
    Rectangle bounds(0.0, 50.0, 0.0, 50.0);
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(bounds),
                             "Distance", DoubleValue(2.0));
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
    Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        uePositionAlloc->Add(Vector(rand->GetValue(0, 50), rand->GetValue(0, 50), 0.0));
    }
    mobility.SetPositionAllocator(uePositionAlloc);
    mobility.Install(ueNodes);

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install IP stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses
    Ptr<EpcHelper> epcHelper = lteHelper->GetEpcHelper();
    Ipv4InterfaceContainer ueIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueDevs.Get(i), enbDevs.Get(0));
    }

    // Install UDP server on eNodeB (on EPC's remote host)
    Ptr<Node> remoteHost = epcHelper->GetRemoteHost();
    InternetStackHelper internetHost;
    internetHost.Install(remoteHost);

    // Assign IPv4 to remoteHost
    Ipv4Address remoteHostAddr;
    {
        PointToPointHelper p2ph;
        p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
        p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
        NetDeviceContainer devs = p2ph.Install(remoteHost, epcHelper->GetPgwNode());
        Ipv4AddressHelper ipv4h;
        ipv4h.SetBase("1.0.0.0", "255.0.0.0");
        Ipv4InterfaceContainer ifaces = ipv4h.Assign(devs);
        remoteHostAddr = ifaces.GetAddress(0);
    }

    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(remoteHost);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP clients on UEs (1 client per UE)
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        UdpClientHelper udpClient(remoteHostAddr, port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(10));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(10.0));
    }

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}