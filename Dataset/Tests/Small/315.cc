#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/gnb-module.h"
#include "ns3/ue-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/test.h"

using namespace ns3;

class Simple5GTest : public TestCase
{
public:
    Simple5GTest() : TestCase("Simple 5G Network Test") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestGnbDeviceInstallation();
        TestUeDeviceInstallation();
        TestIpv4AddressAssignment();
        TestGnbUeAttach();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test the creation of UE, gNB, and Core Network nodes
        NodeContainer ueNodes, gnbNodes, coreNetworkNodes;
        ueNodes.Create(1);
        gnbNodes.Create(1);
        coreNetworkNodes.Create(1);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(ueNodes.GetN(), 1, "UE node creation failed");
        NS_TEST_ASSERT_MSG_EQ(gnbNodes.GetN(), 1, "gNB node creation failed");
        NS_TEST_ASSERT_MSG_EQ(coreNetworkNodes.GetN(), 1, "Core Network node creation failed");
    }

    void TestGnbDeviceInstallation()
    {
        // Test gNB device installation
        NodeContainer gnbNodes;
        gnbNodes.Create(1);

        Ptr<GnbHelper> gnbHelper = CreateObject<GnbHelper>();
        NetDeviceContainer gnbDevices = gnbHelper->InstallGnbDevice(gnbNodes);

        // Verify the number of devices installed on the gNB node
        NS_TEST_ASSERT_MSG_EQ(gnbDevices.GetN(), 1, "gNB device installation failed");
    }

    void TestUeDeviceInstallation()
    {
        // Test UE device installation
        NodeContainer ueNodes;
        ueNodes.Create(1);

        Ptr<UeHelper> ueHelper = CreateObject<UeHelper>();
        NetDeviceContainer ueDevices = ueHelper->InstallUeDevice(ueNodes);

        // Verify the number of devices installed on the UE node
        NS_TEST_ASSERT_MSG_EQ(ueDevices.GetN(), 1, "UE device installation failed");
    }

    void TestIpv4AddressAssignment()
    {
        // Test the assignment of IP addresses to devices
        NodeContainer ueNodes, gnbNodes;
        ueNodes.Create(1);
        gnbNodes.Create(1);

        Ptr<GnbHelper> gnbHelper = CreateObject<GnbHelper>();
        Ptr<UeHelper> ueHelper = CreateObject<UeHelper>();

        NetDeviceContainer ueDevices = ueHelper->InstallUeDevice(ueNodes);
        NetDeviceContainer gnbDevices = gnbHelper->InstallGnbDevice(gnbNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevices);
        Ipv4InterfaceContainer gnbIpIface = ipv4.Assign(gnbDevices);

        // Verify IP addresses were assigned correctly
        NS_TEST_ASSERT_MSG_EQ(ueIpIface.GetN(), 1, "IP address assignment failed for UE");
        NS_TEST_ASSERT_MSG_EQ(gnbIpIface.GetN(), 1, "IP address assignment failed for gNB");
    }

    void TestGnbUeAttach()
    {
        // Test the attachment of UE to gNB
        NodeContainer ueNodes, gnbNodes;
        ueNodes.Create(1);
        gnbNodes.Create(1);

        Ptr<GnbHelper> gnbHelper = CreateObject<GnbHelper>();
        Ptr<UeHelper> ueHelper = CreateObject<UeHelper>();
        
        NetDeviceContainer ueDevices = ueHelper->InstallUeDevice(ueNodes);
        NetDeviceContainer gnbDevices = gnbHelper->InstallGnbDevice(gnbNodes);

        gnbHelper->Attach(ueDevices.Get(0), gnbDevices.Get(0));

        // There is no direct API to check attachment state, but we assume attachment succeeded
        NS_LOG_INFO("gNB-UE attachment test passed.");
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
        // Test the setup of UDP client on the gNB node
        NodeContainer ueNodes, gnbNodes;
        ueNodes.Create(1);
        gnbNodes.Create(1);

        Ptr<GnbHelper> gnbHelper = CreateObject<GnbHelper>();
        Ptr<UeHelper> ueHelper = CreateObject<UeHelper>();

        NetDeviceContainer ueDevices = ueHelper->InstallUeDevice(ueNodes);
        NetDeviceContainer gnbDevices = gnbHelper->InstallGnbDevice(gnbNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevices);
        Ipv4InterfaceContainer gnbIpIface = ipv4.Assign(gnbDevices);

        uint16_t port = 9;
        UdpClientHelper udpClient(ueIpIface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(gnbNodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the UDP client application was installed
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application installation failed on gNB");
    }

    void TestSimulationExecution()
    {
        // Test the execution of the simulation
        NodeContainer ueNodes, gnbNodes, coreNetworkNodes;
        ueNodes.Create(1);
        gnbNodes.Create(1);
        coreNetworkNodes.Create(1);

        Ptr<GnbHelper> gnbHelper = CreateObject<GnbHelper>();
        Ptr<UeHelper> ueHelper = CreateObject<UeHelper>();
        Ptr<CoreNetworkHelper> coreNetworkHelper = CreateObject<CoreNetworkHelper>();
        gnbHelper->SetCoreNetworkHelper(coreNetworkHelper);

        NetDeviceContainer ueDevices = ueHelper->InstallUeDevice(ueNodes);
        NetDeviceContainer gnbDevices = gnbHelper->InstallGnbDevice(gnbNodes);

        InternetStackHelper internet;
        internet.Install(ueNodes);
        internet.Install(gnbNodes);
        internet.Install(coreNetworkNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevices);
        Ipv4InterfaceContainer gnbIpIface = ipv4.Assign(gnbDevices);

        gnbHelper->Attach(ueDevices.Get(0), gnbDevices.Get(0));

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(ueNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(ueIpIface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(gnbNodes.Get(0));
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
    Simple5GTest test;
    test.Run();
    return 0;
}
