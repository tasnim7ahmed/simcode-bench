#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for TCP Example
class TcpExampleTest : public TestCase
{
public:
    TcpExampleTest() : TestCase("Test for TCP Example") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestMobilityModelInstallation();
        TestSocketCreation();
        TestIpAddressAssignment();
        TestTcpServerSocketSetup();
        TestTcpClientSocketSetup();
        TestTcpApplicationSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nNodes = 2;
    NodeContainer nodes;
    Ipv4InterfaceContainer interfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        nodes.Create(nNodes);

        // Verify nodes are created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), nNodes, "Failed to create the expected number of nodes");
    }

    // Test for mobility model installation
    void TestMobilityModelInstallation()
    {
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(5.0),
                                      "GridWidth", UintegerValue(2),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        // Verify mobility model is installed
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<MobilityModel> mobilityModel = nodes.Get(i)->GetObject<MobilityModel>();
            NS_TEST_ASSERT_MSG_NE(mobilityModel, nullptr, "Failed to install mobility model on node");
        }
    }

    // Test for socket creation (server and client)
    void TestSocketCreation()
    {
        TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");

        // Create server socket
        Ptr<Socket> tcpServerSocket = Socket::CreateSocket(nodes.Get(1), tid);
        NS_TEST_ASSERT_MSG_NE(tcpServerSocket, nullptr, "Failed to create server TCP socket");

        // Create client socket
        Ptr<Socket> tcpClientSocket = Socket::CreateSocket(nodes.Get(0), tid);
        NS_TEST_ASSERT_MSG_NE(tcpClientSocket, nullptr, "Failed to create client TCP socket");
    }

    // Test for IP address assignment
    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");

        // Assign IP addresses to nodes
        interfaces = ipv4.Assign(nodes);

        // Verify IP address assignment
        Ipv4Address node1Ip = interfaces.GetAddress(0);
        Ipv4Address node2Ip = interfaces.GetAddress(1);

        NS_TEST_ASSERT_MSG_NE(node1Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 1");
        NS_TEST_ASSERT_MSG_NE(node2Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 2");
    }

    // Test for TCP server socket setup
    void TestTcpServerSocketSetup()
    {
        TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");

        // Create and set up server socket
        Ptr<Socket> tcpServerSocket = Socket::CreateSocket(nodes.Get(1), tid);
        InetSocketAddress serverAddress = InetSocketAddress(Ipv4Address::GetAny(), 8080);
        tcpServerSocket->Bind(serverAddress);
        tcpServerSocket->Listen();
        tcpServerSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

        // Verify server socket setup
        NS_TEST_ASSERT_MSG_EQ(tcpServerSocket->GetLocal(), serverAddress, "TCP server socket setup failed");
    }

    // Test for TCP client socket setup
    void TestTcpClientSocketSetup()
    {
        TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");

        // Create and set up client socket
        Ptr<Socket> tcpClientSocket = Socket::CreateSocket(nodes.Get(0), tid);
        InetSocketAddress clientAddress = InetSocketAddress(interfaces.GetAddress(1), 8080);
        tcpClientSocket->Connect(clientAddress);

        // Verify client socket setup
        NS_TEST_ASSERT_MSG_EQ(tcpClientSocket->GetPeerAddress(), clientAddress, "TCP client socket setup failed");
    }

    // Test for TCP application (client) setup
    void TestTcpApplicationSetup()
    {
        TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");

        // Create application
        Ptr<Socket> appSocket = Socket::CreateSocket(nodes.Get(0), tid);
        Ptr<TcpClient> clientApp = CreateObject<TcpClient>();
        clientApp->Setup(appSocket, InetSocketAddress(interfaces.GetAddress(1), 8080), 1000);
        nodes.Get(0)->AddApplication(clientApp);
        clientApp->SetStartTime(Seconds(1.0));
        clientApp->SetStopTime(Seconds(10.0));

        // Verify that application was added successfully
        NS_TEST_ASSERT_MSG_NE(clientApp, nullptr, "TCP client application setup failed");
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
static TcpExampleTest tcpExampleTestInstance;
