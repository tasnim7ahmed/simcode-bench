#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices1 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices2 = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP Addresses
    Ipv4AddressHelper address1;
    address1.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address1.Assign(devices1);

    Ipv4AddressHelper address2;
    address2.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address2.Assign(devices2);

    // Print Assigned Addresses
    std::cout << "Node 0 interfaces:" << std::endl;
    Ptr<Ipv4> ipv4Node0 = nodes.Get(0)->GetObject<Ipv4>();
    for (uint32_t i = 0; i < ipv4Node0->GetNInterfaces(); ++i)
    {
        std::cout << "  Interface " << i << ": "
                  << ipv4Node0->GetAddress(i, 0).GetLocal() << std::endl;
    }
    std::cout << "Node 1 interfaces:" << std::endl;
    Ptr<Ipv4> ipv4Node1 = nodes.Get(1)->GetObject<Ipv4>();
    for (uint32_t i = 0; i < ipv4Node1->GetNInterfaces(); ++i)
    {
        std::cout << "  Interface " << i << ": "
                  << ipv4Node1->GetAddress(i, 0).GetLocal() << std::endl;
    }
    std::cout << "Node 2 interfaces:" << std::endl;
    Ptr<Ipv4> ipv4Node2 = nodes.Get(2)->GetObject<Ipv4>();
    for (uint32_t i = 0; i < ipv4Node2->GetNInterfaces(); ++i)
    {
        std::cout << "  Interface " << i << ": "
                  << ipv4Node2->GetAddress(i, 0).GetLocal() << std::endl;
    }

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}