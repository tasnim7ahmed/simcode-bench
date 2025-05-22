#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for LteTcpExample
class LteTcpTest : public TestCase
{
public:
    LteTcpTest() : TestCase("Test for LteTcpExample") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestLteStackInstallation();
        TestDeviceInstallation();
        TestIpv4AddressAssignment();
        TestDeviceAttachment();
        TestTcpApplicationSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nUeNodes = 1;
    uint32_t nEnbNodes = 1;
    NodeContainer ueNode;
    NodeContainer enbNode;
    NetDeviceContainer ueDevice;
    NetDeviceContainer enbDevice;
    Ipv4InterfaceContainer ueIpInterface;
    LteHelper lte;
    InternetStackHelper internet;

    // Test for node creation
    void TestNodeCreation()
    {
        ueNode.Create(nUeNodes);
        enbNode.Create(nEnbNodes);

        NS_TEST_ASSERT_MSG_EQ(ueNode.GetN(), nUeNodes, "Failed to create the correct number of UE nodes");
        NS_TEST_ASSERT_MSG_EQ(enbNode.GetN(), nEnbNodes, "Failed to create the correct number of eNB nodes");
    }

    // Test for LTE stack installation
    void TestLteStackInstallation()
    {
        internet.Install(ueNode);
        internet.Install(enbNode);

        // Verify that the Internet stack is installed on both nodes
        NS_TEST_ASSERT_MSG_NE(ueNode.Get(0)->GetObject<Ipv4>(), nullptr, "IPv4 stack not installed on UE node");
        NS_TEST_ASSERT_MSG_NE(enbNode.Get(0)->GetObject<Ipv4>(), nullptr, "IPv4 stack not installed on eNB node");
    }

    // Test for device installation (UE and eNB)
    void TestDeviceInstallation()
    {
        enbDevice = lte.InstallEnbDevice(enbNode);
        ueDevice = lte.InstallUeDevice(ueNode);

        // Verify that devices are installed correctly
        NS_TEST_ASSERT_MSG_EQ(enbDevice.GetN(), 1, "Failed to install eNB device");
        NS_TEST_ASSERT_MSG_EQ(ueDevice.GetN(), 1, "Failed to install UE device");
    }

    // Test for IP address assignment
    void TestIpv4AddressAssignment()
    {
        Ipv4AddressHelper ipv4h;
        ipv4h.SetBase("10.1.1.0", "255.255.255.0");
        ueIpInterface = ipv4h.Assign(ueDevice);

        // Verify that IP addresses are assigned correctly to the UE
        NS_TEST_ASSERT_MSG_NE(ueIpInterface.GetAddress(0), Ipv4Address("0.0.0.0"), "Invalid IPv4 address assigned to UE node");
    }

    // Test for device attachment (UE to eNB)
    void TestDeviceAttachment()
    {
        lte.Attach(ueDevice, enbDevice.Get(0));

        // Verify that UE is attached to eNB
        NS_TEST_ASSERT_MSG_EQ(lte.GetUeDevice(0)->GetObject<NetDevice>(), ueDevice.Get(0), "UE is not attached to eNB");
    }

    // Test for TCP application setup (client and server)
    void TestTcpApplicationSetup()
    {
        uint16_t port = 9;

        // Set up TCP client application on UE (client)
        TcpClientHelper tcpClient(ueIpInterface.GetAddress(0), port);
        tcpClient.SetAttribute("MaxPackets", UintegerValue(100));
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = tcpClient.Install(enbNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the TCP client application is installed on eNB
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "TCP client application not installed on eNB node");

        // Set up TCP server application on eNB (server)
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(ueNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the TCP server application is installed on UE
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "TCP server application not installed on UE node");
    }

    // Test for simulation execution
    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();

        // Verify that the simulation ran and was destroyed without errors
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static LteTcpTest lteTcpTestInstance;
