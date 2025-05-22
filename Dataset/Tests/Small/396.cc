#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for LTE Example
class LteExampleTest : public TestCase
{
public:
    LteExampleTest() : TestCase("Test for LTE Example") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestLteDeviceInstallation();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestAttachUeToEnb();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nEnbNodes = 1;
    uint32_t nUeNodes = 1;
    NodeContainer eNBNodes;
    NodeContainer ueNodes;
    Ipv4InterfaceContainer ueIpIface;

    // Test for node creation
    void TestNodeCreation()
    {
        eNBNodes.Create(nEnbNodes);
        ueNodes.Create(nUeNodes);

        // Verify nodes are created
        NS_TEST_ASSERT_MSG_EQ(eNBNodes.GetN(), nEnbNodes, "Failed to create eNB nodes");
        NS_TEST_ASSERT_MSG_EQ(ueNodes.GetN(), nUeNodes, "Failed to create UE nodes");
    }

    // Test for LTE device installation
    void TestLteDeviceInstallation()
    {
        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        // Install LTE devices
        NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(eNBNodes);
        NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

        // Verify LTE devices are installed correctly
        NS_TEST_ASSERT_MSG_EQ(enbDevs.GetN(), nEnbNodes, "Failed to install LTE devices on eNB");
        NS_TEST_ASSERT_MSG_EQ(ueDevs.GetN(), nUeNodes, "Failed to install LTE devices on UE");
    }

    // Test for Internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(eNBNodes);
        stack.Install(ueNodes);

        // Verify that Internet stack is installed on all nodes
        for (uint32_t i = 0; i < eNBNodes.GetN(); ++i)
        {
            Ptr<Ipv4> ipv4 = eNBNodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Failed to install Internet stack on eNB node");
        }
        for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
        {
            Ptr<Ipv4> ipv4 = ueNodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Failed to install Internet stack on UE node");
        }
    }

    // Test for IP address assignment
    void TestIpAddressAssignment()
    {
        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        // Assign IP addresses to UE
        NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);
        ueIpIface = epcHelper->AssignUeIpv4Address(ueDevs);

        // Verify IP addresses are assigned correctly
        Ipv4Address ueIp = ueIpIface.GetAddress(0);
        NS_TEST_ASSERT_MSG_NE(ueIp, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to UE");
    }

    // Test for attaching UE to eNB
    void TestAttachUeToEnb()
    {
        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        // Install LTE devices
        NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(eNBNodes);
        NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

        // Attach UE to eNB
        lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));

        // Verify attachment
        Ptr<LteUeNetDevice> ueDevice = DynamicCast<LteUeNetDevice>(ueDevs.Get(0));
        Ptr<NetDevice> enbDevice = enbDevs.Get(0);

        NS_TEST_ASSERT_MSG_NE(ueDevice->GetServingEnb(), nullptr, "Failed to attach UE to eNB");
    }

    // Test for UDP server setup on UE
    void TestUdpServerSetup()
    {
        uint16_t port = 12345;
        UdpServerHelper server(port);
        ApplicationContainer serverApp = server.Install(ueNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the server application is installed correctly
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Failed to install UDP server on UE node");
    }

    // Test for UDP client setup on eNB
    void TestUdpClientSetup()
    {
        uint16_t port = 12345;
        UdpClientHelper client(ueIpIface.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(1000));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
        client.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = client.Install(eNBNodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the client application is installed correctly
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "Failed to install UDP client on eNB node");
    }

    // Test for simulation execution
    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();

        // Verify simulation ran and was destroyed without errors
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error during execution");
    }
};

// Register the test
static LteExampleTest lteExampleTestInstance;
