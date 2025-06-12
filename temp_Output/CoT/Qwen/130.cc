#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/nat-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NatSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("Nat", LOG_LEVEL_ALL);

    NodeContainer privateNodes;
    privateNodes.Create(2);

    NodeContainer natNode;
    natNode.Create(1);

    NodeContainer publicServer;
    publicServer.Create(1);

    PointToPointHelper p2pPrivate;
    p2pPrivate.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pPrivate.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pPublic;
    p2pPublic.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pPublic.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer privateDevices;
    NetDeviceContainer natPrivateDevice;
    privateDevices = p2pPrivate.Install(privateNodes.Get(0), natNode.Get(0));
    privateDevices.Add(p2pPrivate.Install(privateNodes.Get(1), natNode.Get(0)));

    NetDeviceContainer natPublicDevice = p2pPublic.Install(natNode.Get(0), publicServer.Get(0));

    InternetStackHelper stack;
    stack.Install(privateNodes);
    stack.Install(natNode);
    stack.Install(publicServer);

    Ipv4AddressHelper address;

    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer privateInterfaces;
    for (uint32_t i = 0; i < privateDevices.GetN(); ++i) {
        privateInterfaces.Add(address.Assign(privateDevices.Get(i)));
    }

    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer natPublicInterface = address.Assign(natPublicDevice.Get(0));

    Ipv4InterfaceContainer serverInterface;
    serverInterface.Add(address.Assign(natPublicDevice.Get(1)));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    NatHelper natHelper;
    natHelper.SetNatNetwork(Ipv4Address("10.0.0.0"), Ipv4Mask("/24"));
    natHelper.AssignIpv4Addresses(Ipv4Address("10.0.0.100"), Ipv4Address("10.0.0.200"));

    natHelper.Install(natNode.Get(0), natPublicInterface.GetInterfaceIndex(), 100);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(publicServer.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(serverInterface.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps1 = echoClient.Install(privateNodes.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(10.0));

    ApplicationContainer clientApps2 = echoClient.Install(privateNodes.Get(1));
    clientApps2.Start(Seconds(3.0));
    clientApps2.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    p2pPublic.EnableAsciiAll(ascii.CreateFileStream("nat_simulation.tr"));
    p2pPrivate.EnableAsciiAll(ascii.CreateFileStream("nat_simulation.tr"));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}