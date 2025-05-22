#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <cassert>

using namespace ns3;

void TestNodeCreation(NodeContainer &nodes) {
    assert(nodes.GetN() == 4 && "Node creation failed");
}

void TestLinkCreation(NetDeviceContainer &devices) {
    assert(devices.GetN() == 2 && "Link creation failed");
}

void TestIpAssignment(Ipv4InterfaceContainer &interfaces) {
    assert(interfaces.GetN() == 2 && "IP assignment failed");
}

void TestUdpEchoApplication(Ptr<Node> serverNode, Ptr<Node> clientNode, Ipv4Address serverIp) {
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(serverNode);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(serverIp, 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = echoClient.Install(clientNode);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    assert(serverApp.GetN() > 0 && clientApp.GetN() > 0 && "UDP Echo application setup failed");
}

void RunTests() {
    NodeContainer tree;
    tree.Create(4);
    TestNodeCreation(tree);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer device1 = pointToPoint.Install(tree.Get(0), tree.Get(1));
    NetDeviceContainer device2 = pointToPoint.Install(tree.Get(0), tree.Get(2));
    NetDeviceContainer device3 = pointToPoint.Install(tree.Get(1), tree.Get(3));

    TestLinkCreation(device1);
    TestLinkCreation(device2);
    TestLinkCreation(device3);

    InternetStackHelper stack;
    stack.Install(tree);

    Ipv4AddressHelper address1, address2, address3;
    address1.SetBase("192.9.39.0", "255.255.255.252");
    address2.SetBase("192.9.39.4", "255.255.255.252");
    address3.SetBase("192.9.39.8", "255.255.255.252");

    Ipv4InterfaceContainer interface1 = address1.Assign(device1);
    Ipv4InterfaceContainer interface2 = address2.Assign(device2);
    Ipv4InterfaceContainer interface3 = address3.Assign(device3);

    TestIpAssignment(interface1);
    TestIpAssignment(interface2);
    TestIpAssignment(interface3);

    TestUdpEchoApplication(tree.Get(0), tree.Get(1), interface1.GetAddress(0));
    TestUdpEchoApplication(tree.Get(1), tree.Get(0), interface1.GetAddress(1));
    TestUdpEchoApplication(tree.Get(0), tree.Get(2), interface2.GetAddress(0));
    TestUdpEchoApplication(tree.Get(2), tree.Get(0), interface2.GetAddress(1));
    TestUdpEchoApplication(tree.Get(1), tree.Get(3), interface3.GetAddress(0));
    TestUdpEchoApplication(tree.Get(3), tree.Get(1), interface3.GetAddress(1));

    Simulator::Run();
    Simulator::Destroy();

    std::cout << "All tests passed successfully!" << std::endl;
}

int main(int argc, char *argv[]) {
    RunTests();
    return 0;
}
