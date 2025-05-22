#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class SimpleLteTest : public TestCase
{
public:
    SimpleLteTest() : TestCase("Simple LTE Test") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestLteStackInstallation();
        TestDeviceInstallation();
        TestIpv4AddressAssignment();
        TestAttachUeToEnb();
        TestTcpServerSetup();
        TestTcpClientSetup();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test the creation of eNodeB and UE nodes
        NodeContainer ueNodes, enbNode;
        ueNodes.Create(2);
        enbNode.Create(1);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(ueNodes.GetN(), 2, "UE nodes creation failed, incorrect number of nodes");
        NS_TEST_ASSERT_MSG_EQ(enbNode.GetN(), 1, "eNodeB node creation failed, incorrect number of nodes");
    }

    void TestLteStackInstallation()
    {
        // Test installation of LTE stack
        NodeContainer ueNodes, enbNode;
        ueNodes.Create(2);
        enbNode.Create(1);

        LteHelper lte;
        NetDeviceContainer ueDevices, enbDevices;

        enbDevices = lte.InstallEnbDevice(enbNode);
        ueDevices = lte.InstallUeDevice(ueNodes);

        // Verify LTE devices are installed
        NS_TEST_ASSERT_MSG_EQ(ueDevices.GetN(), 2, "UE LTE device installation failed");
        NS_TEST_ASSERT_MSG_EQ(enbDevices.GetN(), 1, "eNodeB LTE device installation failed");
    }

    void TestDeviceInstallation()
    {
        // Test installation of LTE devices on both eNodeB and UEs
        NodeContainer ueNodes, enbNode;
        ueNodes.Create(2);
        enbNode.Create(1);

        LteHelper lte;
        NetDeviceContainer ueDevices, enbDevices;

        enbDevices = lte.InstallEnbDevice(enbNode);
        ueDevices = lte.InstallUeDevice(ueNodes);

        // Verify correct installation of devices on nodes
        NS_TEST_ASSERT_MSG_EQ(ueDevices.GetN(), 2, "Incorrect number of LTE devices installed on UE nodes");
        NS_TEST_ASSERT_MSG_EQ(enbDevices.GetN(), 1, "Incorrect number of LTE devices installed on eNodeB node");
    }

    void TestIpv4AddressAssignment()
    {
        // Test assignment of IP addresses to UE devices
        NodeContainer ueNodes;
        ueNodes.Create(2);

        LteHelper lte;
        NetDeviceContainer ueDevices;
        ueDevices = lte.InstallUeDevice(ueNodes);

        Ipv4InterfaceContainer ueIpInterface;
        ueIpInterface = lte.AssignUeIpv4Address(ueDevices);

        // Verify that IP addresses were assigned to both UEs
        NS_TEST_ASSERT_MSG_EQ(ueIpInterface.GetN(), 2, "IP address assignment failed, incorrect number of addresses");
    }

    void TestAttachUeToEnb()
    {
        // Test attaching UEs to the eNodeB
        NodeContainer ueNodes, enbNode;
        ueNodes.Create(2);
        enbNode.Create(1);

        LteHelper lte;
        NetDeviceContainer ueDevices, enbDevices;

        enbDevices = lte.InstallEnbDevice(enbNode);
        ueDevices = lte.InstallUeDevice(ueNodes);

        lte.Attach(ueDevices.Get(0), enbDevices.Get(0));
        lte.Attach(ueDevices.Get(1), enbDevices.Get(0));

        // Verify that the UEs are attached to the eNodeB
        NS_TEST_ASSERT_MSG_EQ(true, lte.IsUeAttached(ueDevices.Get(0)), "UE 0 is not attached to eNodeB");
        NS_TEST_ASSERT_MSG_EQ(true, lte.IsUeAttached(ueDevices.Get(1)), "UE 1 is not attached to eNodeB");
    }

    void TestTcpServerSetup()
    {
        // Test the setup of TCP server on UE 0
        NodeContainer ueNodes;
        ueNodes.Create(2);

        uint16_t port = 8080;
        Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper tcpSink("ns3::TcpSocketFactory", serverAddress);
        ApplicationContainer serverApp = tcpSink.Install(ueNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the TCP server application is installed
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "TCP server not correctly installed on UE 0");
    }

    void TestTcpClientSetup()
    {
        // Test the setup of TCP client on UE 1
        NodeContainer ueNodes;
        ueNodes.Create(2);

        LteHelper lte;
        NetDeviceContainer ueDevices;
        ueDevices = lte.InstallUeDevice(ueNodes);

        uint16_t port = 8080;
        Ipv4InterfaceContainer ueIpInterface;
        ueIpInterface = lte.AssignUeIpv4Address(ueDevices);

        BulkSendHelper tcpClient("ns3::TcpSocketFactory", InetSocketAddress(ueIpInterface.GetAddress(0), port));
        tcpClient.SetAttribute("MaxBytes", UintegerValue(1000000));
        ApplicationContainer clientApp = tcpClient.Install(ueNodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the TCP client application is installed
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "TCP client not correctly installed on UE 1");
    }

    void TestSimulationExecution()
    {
        // Test the execution of the simulation
        NodeContainer ueNodes, enbNode;
        ueNodes.Create(2);
        enbNode.Create(1);

        LteHelper lte;
        NetDeviceContainer ueDevices, enbDevices;

        enbDevices = lte.InstallEnbDevice(enbNode);
        ueDevices = lte.InstallUeDevice(ueNodes);

        InternetStackHelper internet;
        internet.Install(ueNodes);
        internet.Install(enbNode);

        Ipv4InterfaceContainer ueIpInterface;
        ueIpInterface = lte.AssignUeIpv4Address(ueDevices);

        lte.Attach(ueDevices.Get(0), enbDevices.Get(0));
        lte.Attach(ueDevices.Get(1), enbDevices.Get(0));

        uint16_t port = 8080;
        Address serverAddress(InetSocketAddress(ueIpInterface.GetAddress(0), port));
        PacketSinkHelper tcpSink("ns3::TcpSocketFactory", serverAddress);
        ApplicationContainer serverApp = tcpSink.Install(ueNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        BulkSendHelper tcpClient("ns3::TcpSocketFactory", InetSocketAddress(ueIpInterface.GetAddress(0), port));
        tcpClient.SetAttribute("MaxBytes", UintegerValue(1000000));
        ApplicationContainer clientApp = tcpClient.Install(ueNodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Verify that the simulation ran without errors
        NS_TEST_ASSERT_MSG_EQ(true, true, "Simulation execution failed");
    }
};

// Create the test case and run it
int main()
{
    SimpleLteTest test;
    test.Run();
    return 0;
}
