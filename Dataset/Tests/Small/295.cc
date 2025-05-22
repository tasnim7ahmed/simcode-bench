#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/test.h"

using namespace ns3;

class LteUdpExampleTest : public TestCase
{
public:
    LteUdpExampleTest() : TestCase("Test LTE UDP Example") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestLteNetworkSetup();
        TestMobilityInstallation();
        TestIpv4AddressAssignment();
        TestAttachUeToEnb();
        TestUdpServerInstallation();
        TestUdpClientInstallation();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test creation of one eNodeB and one UE
        NodeContainer ueNodes, enbNodes;
        enbNodes.Create(1);
        ueNodes.Create(1);

        // Verify that nodes are created correctly
        NS_TEST_ASSERT_MSG_EQ(ueNodes.GetN(), 1, "UE node creation failed");
        NS_TEST_ASSERT_MSG_EQ(enbNodes.GetN(), 1, "eNodeB node creation failed");
    }

    void TestLteNetworkSetup()
    {
        // Test LTE network setup and device installation
        NodeContainer ueNodes, enbNodes;
        enbNodes.Create(1);
        ueNodes.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
        NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

        // Verify that LTE devices are installed
        NS_TEST_ASSERT_MSG_EQ(enbDevs.GetN(), 1, "eNodeB LTE device installation failed");
        NS_TEST_ASSERT_MSG_EQ(ueDevs.GetN(), 1, "UE LTE device installation failed");
    }

    void TestMobilityInstallation()
    {
        // Test installation of mobility models
        NodeContainer ueNodes, enbNodes;
        enbNodes.Create(1);
        ueNodes.Create(1);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(enbNodes); // eNodeB position
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel");
        mobility.Install(ueNodes); // UE position

        // Verify mobility model installation (basic check on the first node)
        Ptr<MobilityModel> mobilityModel = ueNodes.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_EQ(mobilityModel != nullptr, true, "Mobility model installation failed on UE");
    }

    void TestIpv4AddressAssignment()
    {
        // Test IP address assignment to UE devices
        NodeContainer ueNodes, enbNodes;
        enbNodes.Create(1);
        ueNodes.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
        NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);

        // Verify that the UE device gets an IP address
        Ipv4Address address = ueIpIface.GetAddress(0);
        NS_TEST_ASSERT_MSG_EQ(address.IsValid(), true, "IP address assignment failed for UE");
    }

    void TestAttachUeToEnb()
    {
        // Test that the UE is successfully attached to the eNodeB
        NodeContainer ueNodes, enbNodes;
        enbNodes.Create(1);
        ueNodes.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
        NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

        lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));

        // Verify that the UE is attached to the eNodeB
        bool isAttached = lteHelper->IsUeAttachedToEnb(ueDevs.Get(0));
        NS_TEST_ASSERT_MSG_EQ(isAttached, true, "UE attachment to eNodeB failed");
    }

    void TestUdpServerInstallation()
    {
        // Test UDP server installation on UE
        NodeContainer ueNodes, enbNodes;
        enbNodes.Create(1);
        ueNodes.Create(1);

        uint16_t port = 8080;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(ueNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the server application is installed
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application installation failed");
    }

    void TestUdpClientInstallation()
    {
        // Test UDP client installation on eNodeB
        NodeContainer ueNodes, enbNodes;
        enbNodes.Create(1);
        ueNodes.Create(1);

        uint16_t port = 8080;
        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
        NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);

        UdpClientHelper udpClient(ueIpIface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(enbNodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the client application is installed
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application installation failed");
    }

    void TestSimulationExecution()
    {
        // Test if the simulation runs without errors
        NodeContainer ueNodes, enbNodes;
        enbNodes.Create(1);
        ueNodes.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
        NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);

        uint16_t port = 8080;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(ueNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(ueIpIface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(enbNodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Check if simulation completes successfully
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext()->GetTotalSimTime().GetSeconds() > 0, true, "Simulation did not run correctly");
    }
};

int main(int argc, char *argv[])
{
    // Run the unit tests
    Ptr<LteUdpExampleTest> test = CreateObject<LteUdpExampleTest>();
    test->Run();

    return 0;
}
