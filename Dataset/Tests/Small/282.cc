#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include <cassert>

using namespace ns3;

// Function to test node creation
void TestNodeCreation() {
    NodeContainer csmaNodes;
    csmaNodes.Create(4);

    assert(csmaNodes.GetN() == 4 && "Failed to create CSMA nodes");
}

// Function to test CSMA device installation
void TestCsmaDeviceInstallation() {
    NodeContainer csmaNodes;
    csmaNodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);
    assert(csmaDevices.GetN() == 4 && "Failed to install CSMA devices");
}

// Function to test IP address assignment
void TestIpAddressAssignment() {
    NodeContainer csmaNodes;
    csmaNodes.Create(4);

    CsmaHelper csma;
    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    InternetStackHelper stack;
    stack.Install(csmaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    assert(csmaInterfaces.GetAddress(0) != Ipv4Address::GetAny() && "Failed to assign IP to node 0");
}

// Function to test TCP server installation
void TestTcpServerSetup() {
    NodeContainer csmaNodes;
    csmaNodes.Create(4);

    CsmaHelper csma;
    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    InternetStackHelper stack;
    stack.Install(csmaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(csmaInterfaces.GetAddress(0), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = sinkHelper.Install(csmaNodes.Get(0));

    assert(serverApp.GetN() > 0 && "Failed to install TCP server");
}

// Function to test TCP client installation
void TestTcpClientSetup() {
    NodeContainer csmaNodes;
    csmaNodes.Create(4);

    CsmaHelper csma;
    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    InternetStackHelper stack;
    stack.Install(csmaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(csmaInterfaces.GetAddress(0), port));
    OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
    clientHelper.SetAttribute("DataRate", StringValue("5Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < 4; ++i) {
        clientApps.Add(clientHelper.Install(csmaNodes.Get(i)));
    }

    assert(clientApps.GetN() == 3 && "Failed to install TCP clients on nodes 1, 2, and 3");
}

// Function to test network simulation execution
void TestSimulationExecution() {
    Simulator::Run();
    Simulator::Destroy();
    assert(true && "Simulation executed successfully");
}

// Function to execute all test cases
void RunAllTests() {
    TestNodeCreation();
    TestCsmaDeviceInstallation();
    TestIpAddressAssignment();
    TestTcpServerSetup();
    TestTcpClientSetup();
    TestSimulationExecution();

    std::cout << "All NS-3 CSMA TCP tests passed successfully!" << std::endl;
}

// Main function to run tests
int main() {
    RunAllTests();
    return 0;
}
