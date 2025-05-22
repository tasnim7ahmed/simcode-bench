#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for SimpleLteExample
class SimpleLteTest : public TestCase
{
public:
    SimpleLteTest() : TestCase("Test for Simple LTE Example") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestLteDeviceSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestApplicationSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nEnbNodes = 1;
    uint32_t nUeNodes = 1;
    NodeContainer enbNode, ueNode;
    NetDeviceContainer enbLteDevice, ueLteDevice;
    Ipv4InterfaceContainer ueIpIfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        enbNode.Create(nEnbNodes);
        ueNode.Create(nUeNodes);
        
        NS_TEST_ASSERT_MSG_EQ(enbNode.GetN(), nEnbNodes, "Failed to create the correct number of eNB nodes");
        NS_TEST_ASSERT_MSG_EQ(ueNode.GetN(), nUeNodes, "Failed to create the correct number of UE nodes");
    }

    // Test for LTE device setup
    void TestLteDeviceSetup()
    {
        LteHelper lteHelper;

        enbLteDevice = lteHelper.InstallEnbDevice(enbNode);
        ueLteDevice = lteHelper.InstallUeDevice(ueNode);

        // Verify LTE devices are installed
        NS_TEST_ASSERT_MSG_EQ(enbLteDevice.GetN(), nEnbNodes, "Failed to install LTE devices on eNB nodes");
        NS_TEST_ASSERT_MSG_EQ(ueLteDevice.GetN(), nUeNodes, "Failed to install LTE devices on UE nodes");
    }

    // Test for Internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper internet;
        internet.Install(ueNode);
        internet.Install(enbNode);

        // Verify that the Internet stack is installed on the nodes
        Ptr<Ipv4> ipv4 = ueNode.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on UE node");

        ipv4 = enbNode.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on eNB node");
    }

    // Test for IP address assignment
    void TestIpAddressAssignment()
    {
        LteHelper lteHelper;
        ueIpIfaces = lteHelper.AssignUeIpv4Address(ueLteDevice);

        // Verify that the IP address was assigned correctly
        NS_TEST_ASSERT_MSG_NE(ueIpIfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to UE node");
    }

    // Test for application setup (UDP server and client)
    void TestApplicationSetup()
    {
        uint16_t port = 12345;

        // Set up UDP Echo Server on UE node
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(ueNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up UDP Echo Client on eNB node
        UdpClientHelper udpClient(ueIpIfaces.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(enbNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(9.0));

        // Verify application setup
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP server application not installed on UE node");
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP client application not installed on eNB node");
    }

    // Test for simulation execution
    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static SimpleLteTest simpleLteTestInstance;
