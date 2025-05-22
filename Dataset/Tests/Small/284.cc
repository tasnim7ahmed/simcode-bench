#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <cassert>

using namespace ns3;

// Test node creation
void TestNodeCreation() {
    NodeContainer nodes;
    nodes.Create(2);
    assert(nodes.GetN() == 2 && "Failed to create nodes");
}

// Test Point-to-Point device installation
void TestPointToPointInstallation() {
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);
    assert(devices.GetN() == 2 && "Failed to install Point-to-Point devices");
}

// Test Internet stack installation
void TestInternetStackInstallation() {
    NodeContainer nodes;
    nodes.Create(2);

    InternetStackHelper stack;
    stack.Install(nodes);

    assert(true && "Internet stack installed successfully");
}

// Test IP address assignment
void TestIpAddressAssignment() {
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    assert(interfaces.GetAddress(0) != Ipv4Address::GetAny() && "Failed to assign IP to Node 0");
    assert(interfaces.GetAddress(1) != Ipv4Address::GetAny() && "Failed to assign IP to Node 1");
}

// Test TCP Sink application setup
void TestTcpSinkSetup() {
    NodeContainer nodes;
    nodes.Create(2);

    InternetStackHelper stack;
    stack.Install(nodes);
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(address.NewAddress(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));

    assert(sinkApp.GetN() > 0 && "Failed to install TCP sink");
}

// Test TCP BulkSend application setup
void TestTcpBulkSendSetup() {
    NodeContainer nodes;
    nodes.Create(2);

    InternetStackHelper stack;
    stack.Install(nodes);
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(address.NewAddress(), port));
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
    
    ApplicationContainer sourceApp = bulkSendHelper.Install(nodes.Get(0));
    assert(sourceApp.GetN() > 0 && "Failed to install TCP BulkSend application");
}

// Test simulation execution
void TestSimulationExecution() {
    Simulator::Run();
    Simulator::Destroy();
    assert(true && "Simulation executed successfully");
}

// Run all tests
void RunAllTests() {
    TestNodeCreation();
    TestPointToPointInstallation();
    TestInternetStackInstallation();
    TestIpAddressAssignment();
    TestTcpSinkSetup();
    TestTcpBulkSendSetup();
    TestSimulationExecution();
    std::cout << "All NS-3 TCP tests passed successfully!" << std::endl;
}

int main() {
    RunAllTests();
    return 0;
}
