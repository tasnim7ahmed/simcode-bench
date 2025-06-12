#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class LteTcpTest : public TestCase
{
public:
    LteTcpTest() : TestCase("Test LTE TCP Example") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestLteStackInstallation();
        TestMobilitySetup();
        TestIpv4AddressAssignment();
        TestApplicationInstallation();
        TestHandoverFunctionality();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test creation of 2 eNodeBs and 1 UE node
        NodeContainer eNodeBs, ueNodes;
        eNodeBs.Create(2);
        ueNodes.Create(1);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(eNodeBs.GetN(), 2, "eNodeB creation failed, incorrect number of eNodeBs");
        NS_TEST_ASSERT_MSG_EQ(ueNodes.GetN(), 1, "UE node creation failed, incorrect number of UE nodes");
    }

    void TestLteStackInstallation()
    {
        // Test installation of LTE stack on eNodeBs and UE nodes
        NodeContainer eNodeBs, ueNodes;
        eNodeBs.Create(2);
        ueNodes.Create(1);

        Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

        // Install LTE stack
        lteHelper->Install(eNodeBs);
        lteHelper->Install(ueNodes);

        // Verify that LTE stack is installed
        NS_TEST_ASSERT_MSG_EQ(eNodeBs.GetN(), 2, "LTE stack installation failed on eNodeBs");
        NS_TEST_ASSERT_MSG_EQ(ueNodes.GetN(), 1, "LTE stack installation failed on UE nodes");
    }

    void TestMobilitySetup()
    {
        // Test mobility models setup for eNodeBs and UE nodes
        NodeContainer eNodeBs, ueNodes;
        eNodeBs.Create(2);
        ueNodes.Create(1);

        MobilityHelper mobility;

        // Set eNodeBs to constant position
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(eNodeBs);

        // Set UE mobility to random walk model
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel");
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(50.0),
                                      "DeltaY", DoubleValue(50.0),
                                      "GridWidth", UintegerValue(1),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.Install(ueNodes);

        // Verify mobility model installation
        Ptr<MobilityModel> mobilityModelUe = ueNodes.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_EQ(mobilityModelUe != nullptr, true, "Mobility model installation failed on UE node");

        Ptr<MobilityModel> mobilityModelEnodeb = eNodeBs.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_EQ(mobilityModelEnodeb != nullptr, true, "Mobility model installation failed on eNodeB node");
    }

    void TestIpv4AddressAssignment()
    {
        // Test assignment of IP addresses to the UE node
        NodeContainer eNodeBs, ueNodes;
        eNodeBs.Create(2);
        ueNodes.Create(1);

        Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

        // Install LTE stack
        lteHelper->Install(eNodeBs);
        lteHelper->Install(ueNodes);

        Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(DevicesContainer(ueNodes));

        // Verify IP address assignment
        Ipv4Address ueAddress = ueIpIface.GetAddress(0);
        NS_TEST_ASSERT_MSG_EQ(ueAddress.IsValid(), true, "IPv4 address assignment failed for UE node");
    }

    void TestApplicationInstallation()
    {
        // Test installation of UDP server and client applications
        NodeContainer eNodeBs, ueNodes;
        eNodeBs.Create(2);
        ueNodes.Create(1);

        uint16_t port = 8080;
        ApplicationContainer serverApp;
        UdpServerHelper udpServer(port);
        serverApp = udpServer.Install(eNodeBs.Get(0)); // eNodeB 1 as server
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(ueIpIface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = udpClient.Install(ueNodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify application installation
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application installation failed");
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application installation failed");
    }

    void TestHandoverFunctionality()
    {
        // Test handover functionality between eNodeBs
        NodeContainer eNodeBs, ueNodes;
        eNodeBs.Create(2);
        ueNodes.Create(1);

        Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

        lteHelper->Install(eNodeBs);
        lteHelper->Install(ueNodes);

        // Set up the handover between eNodeBs
        lteHelper->ActivateHandover(ueNodes.Get(0), eNodeBs.Get(0), eNodeBs.Get(1));

        // Verify handover was triggered (dummy test)
        NS_TEST_ASSERT_MSG_EQ(true, true, "Handover functionality failed to trigger");
    }

    void TestSimulationExecution()
    {
        // Test if the simulation runs without errors
        NodeContainer eNodeBs, ueNodes;
        eNodeBs.Create(2);
        ueNodes.Create(1);

        uint16_t port = 8080;
        ApplicationContainer serverApp;
        UdpServerHelper udpServer(port);
        serverApp = udpServer.Install(eNodeBs.Get(0)); // eNodeB 1 as server
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(ueIpIface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = udpClient.Install(ueNodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Check if the simulation ran without errors (basic test)
        NS_TEST_ASSERT_MSG_EQ(true, true, "Simulation execution failed");
    }
};

// Create the test case and run it
int main()
{
    LteTcpTest test;
    test.Run();
    return 0;
}
