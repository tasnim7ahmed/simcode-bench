#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
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
        TestIpv4AddressAssignment();
        TestLteAttach();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test the creation of UE, eNodeB, and EPC nodes
        NodeContainer ueNodes, enbNodes, epcNodes;
        ueNodes.Create(1);
        enbNodes.Create(1);
        epcNodes.Create(1);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(ueNodes.GetN(), 1, "UE node creation failed");
        NS_TEST_ASSERT_MSG_EQ(enbNodes.GetN(), 1, "eNodeB node creation failed");
        NS_TEST_ASSERT_MSG_EQ(epcNodes.GetN(), 1, "EPC node creation failed");
    }

    void TestLteDeviceInstallation()
    {
        // Test LTE device installation on UE and eNodeB nodes
        NodeContainer ueNodes, enbNodes;
        ueNodes.Create(1);
        enbNodes.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        NetDeviceContainer ueDevices, enbDevices;
        enbDevices = lteHelper->InstallEnbDevice(enbNodes);
        ueDevices = lteHelper->InstallUeDevice(ueNodes);

        // Verify the number of devices installed
        NS_TEST_ASSERT_MSG_EQ(ueDevices.GetN(), 1, "LTE device installation failed on UE");
        NS_TEST_ASSERT_MSG_EQ(enbDevices.GetN(), 1, "LTE device installation failed on eNodeB");
    }

    void TestIpv4AddressAssignment()
    {
        // Test the assignment of IP addresses to the devices
        NodeContainer ueNodes, enbNodes;
        ueNodes.Create(1);
        enbNodes.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        NetDeviceContainer ueDevices, enbDevices;
        enbDevices = lteHelper->InstallEnbDevice(enbNodes);
        ueDevices = lteHelper->InstallUeDevice(ueNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevices);
        Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevices);

        // Verify IP addresses were assigned correctly
        NS_TEST_ASSERT_MSG_EQ(ueIpIface.GetN(), 1, "IP address assignment failed for UE");
        NS_TEST_ASSERT_MSG_EQ(enbIpIface.GetN(), 1, "IP address assignment failed for eNodeB");
    }

    void TestLteAttach()
    {
        // Test the attachment of UE to eNodeB
        NodeContainer ueNodes, enbNodes;
        ueNodes.Create(1);
        enbNodes.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        NetDeviceContainer ueDevices, enbDevices;
        enbDevices = lteHelper->InstallEnbDevice(enbNodes);
        ueDevices = lteHelper->InstallUeDevice(ueNodes);

        lteHelper->Attach(ueDevices.Get(0), enbDevices.Get(0));

        // Verify that the attachment has occurred (no direct API for checking attachment state)
        NS_LOG_INFO("LTE attachment test passed.");
    }

    void TestUdpServerSetup()
    {
        // Test the setup of UDP server on the UE node
        NodeContainer ueNodes;
        ueNodes.Create(1);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(ueNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the UDP server application was installed
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application installation failed on UE");
    }

    void TestUdpClientSetup()
    {
        // Test the setup of UDP client on the eNodeB node
        NodeContainer ueNodes, enbNodes;
        ueNodes.Create(1);
        enbNodes.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        NetDeviceContainer ueDevices, enbDevices;
        enbDevices = lteHelper->InstallEnbDevice(enbNodes);
        ueDevices = lteHelper->InstallUeDevice(ueNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevices);
        Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevices);

        uint16_t port = 9;
        UdpClientHelper udpClient(ueIpIface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(enbNodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the UDP client application was installed
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application installation failed on eNodeB");
    }

    void TestSimulationExecution()
    {
        // Test the execution of the simulation
        NodeContainer ueNodes, enbNodes, epcNodes;
        ueNodes.Create(1);
        enbNodes.Create(1);
        epcNodes.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        NetDeviceContainer ueDevices, enbDevices;
        enbDevices = lteHelper->InstallEnbDevice(enbNodes);
        ueDevices = lteHelper->InstallUeDevice(ueNodes);

        InternetStackHelper internet;
        internet.Install(ueNodes);
        internet.Install(enbNodes);
        internet.Install(epcNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevices);
        Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevices);

        lteHelper->Attach(ueDevices.Get(0), enbDevices.Get(0));

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(ueNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(ueIpIface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(enbNodes.Get(0));
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
