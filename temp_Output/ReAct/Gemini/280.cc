#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer gnbNodes;
    gnbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(2);

    NrHelper nrHelper;
    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2pHelper.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer gnbDevs;
    NetDeviceContainer ueDevs;

    for (int i = 0; i < (int) ueNodes.GetN(); ++i) {
        NetDeviceContainer pair = nrHelper.InstallUeDevice(ueNodes.Get(i), gnbNodes.Get(0));
        ueDevs.Add(pair.Get(0));
        gnbDevs.Add(pair.Get(1));
    }

    InternetStackHelper internet;
    internet.Install(gnbNodes);
    internet.Install(ueNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);
    Ipv4InterfaceContainer gnbIpIface = ipv4.Assign(gnbDevs);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("25.0"),
                                  "Y", StringValue("25.0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=25.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));

    mobility.Install(ueNodes);

    Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
    mobility.AssignStreams(var->CreateStreams(0, 1));

    uint16_t port = 8080;
    Address serverAddress (InetSocketAddress (ueIpIface.GetAddress (1), port));

    TcpEchoClientHelper clientHelper (serverAddress);
    clientHelper.SetAttribute ("MaxPackets", UintegerValue (100));
    clientHelper.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
    clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps = clientHelper.Install (ueNodes.Get (0));
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (9.0));

    TcpEchoServerHelper serverHelper (port);
    ApplicationContainer serverApps = serverHelper.Install (ueNodes.Get (1));
    serverApps.Start (Seconds (0.0));
    serverApps.Stop (Seconds (10.0));


    Simulator::Stop(Seconds(10));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}