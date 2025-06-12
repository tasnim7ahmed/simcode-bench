#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleIpAssignment");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);  // Create 3 nodes

    // Install Internet Stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses using a simple point-to-point helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[3];
    Ipv4AddressHelper address;

    // Create point-to-point links between node 0 - 1, and 1 - 2
    devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1));
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces0 = address.Assign(devices[0]);

    devices[1] = p2p.Install(nodes.Get(1), nodes.Get(2));
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices[1]);

    // Assign unique IP to each node directly (loopback or other interface if needed)
    Ptr<Node> node0 = nodes.Get(0);
    Ptr<NetDevice> dummyDevice0 = CreateObject<VirtualNetDevice>();
    node0->AddDevice(dummyDevice0);
    Ipv4Address addressNode0 = "192.168.0.1";
    Ipv4AddressHelper::NewNetwork();
    Ipv4InterfaceAddress ifaceAddr0(addressNode0, Ipv4Mask("/32"));
    node0->GetObject<Ipv4>()->GetInterface(1)->AddAddress(ifaceAddr0);

    Ptr<Node> node1 = nodes.Get(1);
    Ptr<NetDevice> dummyDevice1 = CreateObject<VirtualNetDevice>();
    node1->AddDevice(dummyDevice1);
    Ipv4Address addressNode1 = "192.168.0.2";
    Ipv4InterfaceAddress ifaceAddr1(addressNode1, Ipv4Mask("/32"));
    node1->GetObject<Ipv4>()->GetInterface(1)->AddAddress(ifaceAddr1);

    Ptr<Node> node2 = nodes.Get(2);
    Ptr<NetDevice> dummyDevice2 = CreateObject<VirtualNetDevice>();
    node2->AddDevice(dummyDevice2);
    Ipv4Address addressNode2 = "192.168.0.3";
    Ipv4InterfaceAddress ifaceAddr2(addressNode2, Ipv4Mask("/32"));
    node2->GetObject<Ipv4>()->GetInterface(1)->AddAddress(ifaceAddr2);

    // Print assigned IP addresses
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        uint32_t nInterfaces = ipv4->GetNInterfaces();
        std::cout << "Node " << i << " IP Addresses:" << std::endl;
        for (uint32_t j = 0; j < nInterfaces; ++j) {
            uint32_t nAddresses = ipv4->GetNAddresses(j);
            for (uint32_t k = 0; k < nAddresses; ++k) {
                Ipv4Address addr = ipv4->GetAddress(j, k).GetLocal();
                std::cout << "  Interface " << j << ", Address " << k << ": " << addr << std::endl;
            }
        }
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}