#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-nat.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NatSimulation");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Enable logging for UDP client and server
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer privateNodes;
    privateNodes.Create(2);

    NodeContainer natRouter;
    natRouter.Create(1);

    NodeContainer publicServer;
    publicServer.Create(1);

    // Create point-to-point channels
    PointToPointHelper privateNetwork;
    privateNetwork.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    privateNetwork.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper publicNetwork;
    publicNetwork.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    publicNetwork.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(privateNodes);
    stack.Install(natRouter);
    stack.Install(publicServer);

    // Create NetDevices and addresses for the private network
    NetDeviceContainer privateDevices = privateNetwork.Install(privateNodes, natRouter.Get(0));
    Ipv4AddressHelper privateAddress;
    privateAddress.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer privateInterfaces = privateAddress.Assign(privateDevices);

    // Create NetDevices and addresses for the public network
    NetDeviceContainer publicDevices = publicNetwork.Install(natRouter.Get(0), publicServer.Get(0));
    Ipv4AddressHelper publicAddress;
    publicAddress.SetBase("20.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer publicInterfaces = publicAddress.Assign(publicDevices);

    // Configure NAT router
    Ptr<Node> natNode = natRouter.Get(0);
    Ptr<Ipv4> ipv4 = natNode->GetObject<Ipv4>();
    int publicInterfaceIndex = ipv4->GetInterfaceForDevice(publicDevices.Get(0));
    Ipv4Address publicIp = ipv4->GetAddress(publicInterfaceIndex, 0).GetLocal();

    Ptr<Ipv4Nat> nat = CreateObject<Ipv4Nat>();
    natNode->AddApplication(nat);
    nat->SetPublicInterface(publicInterfaceIndex);
    nat->SetPrivateAddress(privateAddress.GetNetwork());

    // Set up static route for private network
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create UDP client and server
    UdpClientServerHelper udpClient(publicInterfaces.GetAddress(1), 9); // Server address and port
    udpClient.SetAttribute("MaxPackets", UintegerValue(10));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = udpClient.Install(privateNodes.Get(0)); // Install on one of the private nodes
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    ApplicationContainer serverApps = udpClient.Install(publicServer.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(11.0));


    // Set up tracing (optional)
    // PointToPointHelper::EnablePcapAll("nat-simulation");

    // Animation Interface
    // AnimationInterface anim("nat-animation.xml");
    // anim.SetConstantPosition(privateNodes.Get(0), 10.0, 10.0);
    // anim.SetConstantPosition(privateNodes.Get(1), 20.0, 10.0);
    // anim.SetConstantPosition(natRouter.Get(0), 15.0, 20.0);
    // anim.SetConstantPosition(publicServer.Get(0), 15.0, 30.0);

    // Run simulation
    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}