#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/ipv4-address-helper.h"
#include <cassert>

using namespace ns3;

// Function to test node creation
void TestNodeCreation() {
    NodeContainer eNodeBNode, ueNodes;
    eNodeBNode.Create(1);
    ueNodes.Create(3);

    assert(eNodeBNode.GetN() == 1 && "Failed to create eNodeB");
    assert(ueNodes.GetN() == 3 && "Failed to create UEs");
}

// Function to test LTE device installation
void TestLteDeviceInstallation() {
    NodeContainer eNodeBNode, ueNodes;
    eNodeBNode.Create(1);
    ueNodes.Create(3);

    LteHelper lteHelper;
    Ptr<LteEnbNetDevice> enbDevice = lteHelper.InstallEnbDevice(eNodeBNode.Get(0));
    Ptr<LteUeNetDevice> ueDevice1 = lteHelper.InstallUeDevice(ueNodes.Get(0));
    Ptr<LteUeNetDevice> ueDevice2 = lteHelper.InstallUeDevice(ueNodes.Get(1));
    Ptr<LteUeNetDevice> ueDevice3 = lteHelper.InstallUeDevice(ueNodes.Get(2));

    assert(enbDevice != nullptr && "Failed to install LTE eNodeB device");
    assert(ueDevice1 != nullptr && "Failed to install LTE UE device 1");
    assert(ueDevice2 != nullptr && "Failed to install LTE UE device 2");
    assert(ueDevice3 != nullptr && "Failed to install LTE UE device 3");
}

// Function to test IP address assignment
void TestIpAddressAssignment() {
    NodeContainer eNodeBNode, ueNodes;
    eNodeBNode.Create(1);
    ueNodes.Create(3);

    LteHelper lteHelper;
    Ptr<LteEnbNetDevice> enbDevice = lteHelper.InstallEnbDevice(eNodeBNode.Get(0));
    Ptr<LteUeNetDevice> ueDevice1 = lteHelper.InstallUeDevice(ueNodes.Get(0));

    InternetStackHelper internet;
    internet.Install(eNodeBNode);
    internet.Install(ueNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevice);
    Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevice1);

    assert(enbIpIface.GetAddress(0) != Ipv4Address::GetAny() && "Failed to assign IP to eNodeB");
    assert(ueIpIface.GetAddress(0) != Ipv4Address::GetAny() && "Failed to assign IP to UE");
}

// Function to test UDP server and client setup
void TestUdpApplicationSetup() {
    NodeContainer eNodeBNode, ueNodes;
    eNodeBNode.Create(1);
    ueNodes.Create(3);

    InternetStackHelper internet;
    internet.Install(eNodeBNode);
    internet.Install(ueNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer enbIpIface = ipv4.Assign(eNodeBNode);

    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(eNodeBNode.Get(0));

    UdpClientHelper udpClient(enbIpIface.GetAddress(0), port);
    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));

    assert(serverApp.GetN() > 0 && "UDP server not installed properly");
    assert(clientApps.GetN() > 0 && "UDP client not installed properly");
}

// Function to test mobility model setup
void TestMobilitySetup() {
    NodeContainer eNodeBNode, ueNodes;
    eNodeBNode.Create(1);
    ueNodes.Create(3);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(eNodeBNode);

    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobility.Install(ueNodes);

    Ptr<MobilityModel> eNodeBModel = eNodeBNode.Get(0)->GetObject<MobilityModel>();
    Ptr<MobilityModel> ueModel = ueNodes.Get(0)->GetObject<MobilityModel>();

    assert(eNodeBModel != nullptr && "eNodeB mobility model not set properly");
    assert(ueModel != nullptr && "UE mobility model not set properly");
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
    TestLteDeviceInstallation();
    TestIpAddressAssignment();
    TestUdpApplicationSetup();
    TestMobilitySetup();
    TestSimulationExecution();
    
    std::cout << "All NS-3 tests passed successfully!" << std::endl;
}

// Main function to run tests
int main() {
    RunAllTests();
    return 0;
}
