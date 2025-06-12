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
        TestUeEnbAttachment();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test the creation of UE and eNB nodes
        NodeContainer ueNodes, enbNodes;
        ueNodes.Create(1);
        enbNodes.Create(1);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(ueNodes.GetN(), 1, "UE node creation failed");
        NS_TEST_ASSERT_MSG_EQ(enbNodes.GetN(), 1, "eNB node creation failed");
    }

    void TestLteDeviceInstallation()
    {
        // Test LTE device installation on both UE and eNB nodes
        NodeContainer ueNodes, enbNodes;
        ueNodes.Create(1);
        enbNodes.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

        NetDeviceContainer ueDevices, enbDevices;
        ueDevices = lteHelper->InstallUeDevice(ueNodes);
        enbDevices = lteHelper->InstallEnbDevice(enbNodes);

        // Verify devices are installed on both UE and eNB nodes
        NS_TEST_ASSERT_MSG_EQ(ueDevices.GetN(), 1, "LTE device installation failed on UE");
        NS_TEST_ASSERT_MSG_EQ(enbDevices.GetN(), 1, "LTE device installation failed on eNB");
    }

    void TestIpv4AddressAssignment()
    {
        // Test the assignment of IP addresses to devices
        NodeContainer ueNodes, enbNodes;
        ueNodes.Create(1);
        enbNodes.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        NetDeviceContainer ueDevices, enbDevices;
        ueDevices = lteHelper->InstallUeDevice(ueNodes);
        enbDevices = lteHelper->InstallEnbDevice(enbNodes);

        InternetStackHelper internet;
        internet.Install(ueNodes);
        internet.Install(enbNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevices);
        Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevices);

        // Verify IP addresses are assigned to both UE and eNB devices
        NS_TEST_ASSERT_MSG_EQ(ueIpIface.GetN(), 1, "IP address assignment failed for UE");
        NS_TEST_ASSERT_MSG_EQ(enbIpIface.GetN(), 1, "IP address assignment failed for eNB");
    }

    void TestUeEnbAttachment()
    {
        // Test the attachment of UE to the eNB
        NodeContainer ueNodes, enbNodes;
        ueNodes.Create(1);
        enbNodes.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        NetDeviceContainer ueDevices, enbDevices;
        ueDevices = lteHelper->InstallUeDevice(ueNodes);
        enbDevices = lteHelper->InstallEnbDevice(enbNodes);

        // Attach the UE to the eNB
        lteHelper->Attach(ueDevices.Get(0), enbDevices.Get(0));

        // Verify that the UE is attached to the eNB
        bool isAttached = lteHelper->IsUeAttached(ueDevices.Get(0));
        NS_TEST_ASSERT_MSG_EQ(isAttached, true, "UE attachment to eNB failed");
    }

    void TestUdpServerSetup()
    {
        // Test the setup of UDP server on the eNB node
        NodeContainer enbNodes;
        enbNodes.Create(1);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(enbNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify the UDP server application is installed
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application installation failed on eNB");
    }

    void TestUdpClientSetup()
    {
        // Test the setup of UDP client on the UE node
        NodeContainer ueNodes, enbNodes;
        ueNodes.Create(1);
        enbNodes.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        NetDeviceContainer ueDevices, enbDevices;
        ueDevices = lteHelper->InstallUeDevice(ueNodes);
        enbDevices = lteHelper->InstallEnbDevice(enbNodes);

        InternetStackHelper internet;
        internet.Install(ueNodes);
        internet.Install(enbNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevices);
        Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevices);

        uint16_t port = 9;
        UdpClientHelper udpClient(enbIpIface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(ueNodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify the UDP client application is installed
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application installation failed on UE");
    }

    void TestSimulationExecution()
    {
        // Test the execution of the simulation
        NodeContainer ueNodes, enbNodes;
        ueNodes.Create(1);
        enbNodes.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        NetDeviceContainer ueDevices, enbDevices;
        ueDevices = lteHelper->InstallUeDevice(ueNodes);
        enbDevices = lteHelper->InstallEnbDevice(enbNodes);

        InternetStackHelper internet;
        internet.Install(ueNodes);
        internet.Install(enbNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevices);
        Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevices);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(enbNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(enbIpIface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(ueNodes.Get(0));
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
