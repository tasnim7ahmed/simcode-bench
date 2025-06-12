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
    LogComponentEnable("NatHelper", LOG_LEVEL_ALL);

    // Create nodes
    NodeContainer privateNodes;
    privateNodes.Create(2);  // Two hosts in the private network

    NodeContainer natNode;
    natNode.Create(1);       // NAT router node

    NodeContainer publicNode;
    publicNode.Create(1);    // External server node

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Private network (between private nodes and NAT)
    NetDeviceContainer privateDevices[2];
    for (uint32_t i = 0; i < privateNodes.GetN(); ++i) {
        privateDevices[i] = p2p.Install(privateNodes.Get(i), natNode.Get(0));
    }

    // Public network (between NAT and public server)
    NetDeviceContainer publicDevice = p2p.Install(natNode.Get(0), publicNode.Get(0));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(privateNodes);
    stack.Install(natNode);
    stack.Install(publicNode);

    // Assign IP addresses
    Ipv4AddressHelper address;

    // Private network IPs (192.168.x.0/24)
    Ipv4InterfaceContainer privateInterfaces[2];
    address.SetBase("192.168.1.0", "255.255.255.0");
    privateInterfaces[0] = address.Assign(privateDevices[0]);
    address.NewNetwork();
    privateInterfaces[1] = address.Assign(privateDevices[1]);

    // Public network IP (for NAT and server)
    address.SetBase("203.0.113.0", "255.255.255.0");
    Ipv4InterfaceContainer publicInterface = address.Assign(publicDevice);

    // Set default routes
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure NAT on the NAT node
    NatHelper nat;
    nat.SetNatIp(publicInterface.GetAddress(0)); // Public IP of NAT
    nat.Assign(natNode.Get(0)->GetDevice(2));    // Public interface device index (last installed)

    // Setup applications
    uint16_t port = 9;  // UDP echo port

    // Server on public network
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(publicNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Clients on private network
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < privateNodes.GetN(); ++i) {
        UdpEchoClientHelper client(publicInterface.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(2));
        client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer app = client.Install(privateNodes.Get(i));
        app.Start(Seconds(2.0 + i));  // Staggered start times
        app.Stop(Seconds(10.0));
        clientApps.Add(app);
    }

    // Enable pcap tracing for all devices to verify packet flow
    p2p.EnablePcapAll("nat_simulation");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}