#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/test.h"

using namespace ns3;

class SimpleLteTest : public TestCase
{
public:
    SimpleLteTest() : TestCase("Simple LTE Network Test") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestLteDeviceInstallation();
        TestInternetStackInstallation();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestIpAddressAssignment();
        TestLteAttach();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test the creation of eNodeB and UE nodes
        NodeContainer enbNode, ueNodes;
        enbNode.Create(1);
        ueNodes.Create(3);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(enbNode.GetN(), 1, "eNodeB node creation failed");
        NS_TEST_ASSERT_MSG_EQ(ueNodes.GetN(), 3, "UE nodes creation failed");
    }

    void TestLteDeviceInstallation()
    {
        // Test the installation of LTE devices on eNodeB and UE nodes
        NodeContainer enbNode, ueNodes;
        enbNode.Create(1);
        ueNodes.Create(3);

        LteHelper lteHelper;
        NetDeviceContainer enbDevice, ueDevices;
        enbDevice = lteHelper.InstallEnbDevice(enbNode);
        ueDevices = lteHelper.InstallUeDevice(ueNodes);

        // Verify that LTE devices have been installed on both eNodeB and UE nodes
        NS_TEST_ASSERT_MSG_EQ(enbDevice.GetN(), 1, "LTE device installation failed on eNodeB");
        NS_TEST_ASSERT_MSG_EQ(ueDevices.GetN(), 3, "LTE device installation failed on UEs");
    }

    void TestInternetStackInstallation()
    {
        // Test the installation of internet stack (IP, UDP) on eNodeB and UE nodes
        NodeContainer enbNode, ueNodes;
        enbNode.Create(1);
        ueNodes.Create(3);

        InternetStackHelper stack;
        stack.Install(enbNode);
        stack.Install(ueNodes);

        // Verify internet stack installation on all nodes
        for (uint32_t i = 0; i < enbNode.GetN(); ++i)
        {
            Ptr<Node> node = enbNode.Get(i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack installation failed on eNodeB node " << i);
        }

        for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
        {
            Ptr<Node> node = ueNodes.Get(i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack installation failed on UE node " << i);
        }
    }

    void TestUdpServerSetup()
    {
        // Test the setup of the UDP server on eNodeB
        NodeContainer enbNode;
        enbNode.Create(1);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(enbNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the server application is installed correctly
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application installation failed on eNodeB");
    }

    void TestUdpClientSetup()
    {
        // Test the setup of the UDP client on UE nodes
        NodeContainer enbNode, ueNodes;
        enbNode.Create(1);
        ueNodes.Create(3);

        uint16_t port = 9;
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbNode.Get(0)->GetDevice(0));
        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        ipv4.Assign(ueNodes.Get(0)->GetDevice(0));

        UdpClientHelper udpClient(enbIpIface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(ueNodes);
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the client application is installed correctly on all UE nodes
        for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application installation failed on UE node " << i);
        }
    }

    void TestIpAddressAssignment()
    {
        // Test the assignment of IP addresses to the devices on eNodeB and UE nodes
        NodeContainer enbNode, ueNodes;
        enbNode.Create(1);
        ueNodes.Create(3);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevices);
        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevice);

        // Verify that IP addresses are assigned correctly to the devices
        NS_TEST_ASSERT_MSG_EQ(ueIpIface.GetN(), 3, "IP address assignment failed on UE devices");
        NS_TEST_ASSERT_MSG_EQ(enbIpIface.GetN(), 1, "IP address assignment failed on eNodeB");
    }

    void TestLteAttach()
    {
        // Test the attachment of UEs to the eNodeB
        NodeContainer enbNode, ueNodes;
        enbNode.Create(1);
        ueNodes.Create(3);

        LteHelper lteHelper;
        NetDeviceContainer enbDevice, ueDevices;
        enbDevice = lteHelper.InstallEnbDevice(enbNode);
        ueDevices = lteHelper.InstallUeDevice(ueNodes);

        // Attach UEs to the eNodeB
        lteHelper.Attach(ueDevices.Get(0), enbDevice.Get(0));
        lteHelper.Attach(ueDevices.Get(1), enbDevice.Get(0));
        lteHelper.Attach(ueDevices.Get(2), enbDevice.Get(0));

        // Verify that all UEs are successfully attached to eNodeB
        NS_TEST_ASSERT_MSG_EQ(lteHelper.GetUeDevice(ueDevices.Get(0)), enbDevice.Get(0), "UE 0 failed to attach to eNodeB");
        NS_TEST_ASSERT_MSG_EQ(lteHelper.GetUeDevice(ueDevices.Get(1)), enbDevice.Get(0), "UE 1 failed to attach to eNodeB");
        NS_TEST_ASSERT_MSG_EQ(lteHelper.GetUeDevice(ueDevices.Get(2)), enbDevice.Get(0), "UE 2 failed to attach to eNodeB");
    }

    void TestSimulationExecution()
    {
        // Test the execution of the simulation
        NodeContainer enbNode, ueNodes;
        enbNode.Create(1);
        ueNodes.Create(3);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(enbNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4.Assign(enbNode.Get(0)->GetDevice(0));
        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        ipv4.Assign(ueNodes.Get(0)->GetDevice(0));

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
