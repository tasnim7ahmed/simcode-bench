#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer ueNodes;
    NodeContainer enbNodes;
    ueNodes.Create(1);
    enbNodes.Create(1);

    PointToPointEpcHelper epcHelper;
    Ptr<Node> pgw = epcHelper.GetPgwNode();

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));

    NetDeviceContainer enbDev = epcHelper.InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDev = epcHelper.InstallUeDevice(ueNodes);

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface = epcHelper.AssignUeIpv4Address(ueDev);
    Ipv4InterfaceContainer enbIpIface = epcHelper.AssignEnbIpv4Address(enbDev);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
        Ptr<Node> ueNode = ueNodes.Get(u);
        Ptr<NetDevice> uend = ueDev.Get(u);
        Ptr<Ipv4> ueipv4 = ueNode->GetObject<Ipv4>();
        uint32_t interface = ueipv4->GetInterfaceForDevice(uend);
        ipv4RoutingHelper.AddNetworkRouteTo(ueipv4, Ipv4Address("7.0.0.0"), Ipv4Mask("255.255.255.0"), 1);
    }

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(enbIpIface.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100000000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}