#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

// Define the test suite
class TcpRoutedTestSuite : public TestCase {
public:
    TcpRoutedTestSuite() : TestCase("Test TCP Routed Example") {}
    virtual ~TcpRoutedTestSuite() {}

    void DoRun() override {
        TestNodeCreation();
        TestPointToPointSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestRoutingConfiguration();
        TestSocketCreation();
        TestTcpTransmission();
        TestTcpReception();
    }

private:
    uint16_t port = 8080;

    // Test if nodes (Client, Router, Server) are created correctly
    void TestNodeCreation() {
        NodeContainer clientRouter, routerServer;
        clientRouter.Create(2); // Client and Router
        routerServer.Add(clientRouter.Get(1)); // Reuse Router node
        routerServer.Create(1); // Server

        NS_TEST_ASSERT_MSG_EQ(clientRouter.GetN(), 2, "Client-Router node creation failed.");
        NS_TEST_ASSERT_MSG_EQ(routerServer.GetN(), 2, "Router-Server node creation failed.");
    }

    // Test if Point-to-Point links are set up correctly
    void TestPointToPointSetup() {
        NodeContainer clientRouter, routerServer;
        clientRouter.Create(2);
        routerServer.Add(clientRouter.Get(1));
        routerServer.Create(1);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer clientRouterDevices = p2p.Install(clientRouter);
        NetDeviceContainer routerServerDevices = p2p.Install(routerServer);

        NS_TEST_ASSERT_MSG_GT(clientRouterDevices.GetN(), 0, "Point-to-Point setup failed between Client and Router.");
        NS_TEST_ASSERT_MSG_GT(routerServerDevices.GetN(), 0, "Point-to-Point setup failed between Router and Server.");
    }

    // Test if the Internet stack is installed correctly
    void TestInternetStackInstallation() {
        NodeContainer clientRouter, routerServer;
        clientRouter.Create(2);
        routerServer.Add(clientRouter.Get(1));
        routerServer.Create(1);

        InternetStackHelper internet;
        internet.Install(clientRouter);
        internet.Install(routerServer.Get(1)); // Server

        for (uint32_t i = 0; i < clientRouter.GetN(); i++) {
            Ptr<Ipv4> ipv4 = clientRouter.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack installation failed on Client-Router.");
        }
        Ptr<Ipv4> ipv4Server = routerServer.Get(1)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4Server, "Internet stack installation failed on Server.");
    }

    // Test if IP addresses are assigned correctly
    void TestIpAddressAssignment() {
        NodeContainer clientRouter, routerServer;
        clientRouter.Create(2);
        routerServer.Add(clientRouter.Get(1));
        routerServer.Create(1);

        PointToPointHelper p2p;
        NetDeviceContainer clientRouterDevices = p2p.Install(clientRouter);
        NetDeviceContainer routerServerDevices = p2p.Install(routerServer);

        InternetStackHelper internet;
        internet.Install(clientRouter);
        internet.Install(routerServer.Get(1));

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer clientRouterInterfaces = address.Assign(clientRouterDevices);

        address.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer routerServerInterfaces = address.Assign(routerServerDevices);

        NS_TEST_ASSERT_MSG_GT(clientRouterInterfaces.GetN(), 0, "IP assignment failed for Client-Router.");
        NS_TEST_ASSERT_MSG_GT(routerServerInterfaces.GetN(), 0, "IP assignment failed for Router-Server.");
    }

    // Test if routing tables are properly populated
    void TestRoutingConfiguration() {
        NodeContainer clientRouter, routerServer;
        clientRouter.Create(2);
        routerServer.Add(clientRouter.Get(1));
        routerServer.Create(1);

        InternetStackHelper internet;
        internet.Install(clientRouter);
        internet.Install(routerServer.Get(1));

        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        Ptr<Ipv4> routerIpv4 = routerServer.Get(0)->GetObject<Ipv4>();
        Ptr<Ipv4RoutingTableEntry> route = routerIpv4->GetRoutingTableEntry(0);

        NS_TEST_ASSERT_MSG_NOT_NULL(route, "Routing table configuration failed.");
    }

    // Test if TCP sockets are created for client and server
    void TestSocketCreation() {
        NodeContainer clientRouter, routerServer;
        clientRouter.Create(2);
        routerServer.Add(clientRouter.Get(1));
        routerServer.Create(1);

        Ptr<Socket> clientSocket = Socket::CreateSocket(clientRouter.Get(0), TcpSocketFactory::GetTypeId());
        NS_TEST_ASSERT_MSG_NOT_NULL(clientSocket, "TCP client socket creation failed.");

        Ptr<Socket> serverSocket = Socket::CreateSocket(routerServer.Get(1), TcpSocketFactory::GetTypeId());
        NS_TEST_ASSERT_MSG_NOT_NULL(serverSocket, "TCP server socket creation failed.");
    }

    // Test if the client transmits TCP packets correctly
    void TestTcpTransmission() {
        NodeContainer clientRouter, routerServer;
        clientRouter.Create(2);
        routerServer.Add(clientRouter.Get(1));
        routerServer.Create(1);

        PointToPointHelper p2p;
        NetDeviceContainer clientRouterDevices = p2p.Install(clientRouter);
        NetDeviceContainer routerServerDevices = p2p.Install(routerServer);

        InternetStackHelper internet;
        internet.Install(clientRouter);
        internet.Install(routerServer.Get(1));

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer clientRouterInterfaces = address.Assign(clientRouterDevices);

        address.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer routerServerInterfaces = address.Assign(routerServerDevices);

        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        Address serverAddress(InetSocketAddress(routerServerInterfaces.GetAddress(1), port));
        BulkSendHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
        clientHelper.SetAttribute("MaxBytes", UintegerValue(1024)); // Send 1024 bytes

        ApplicationContainer clientApp = clientHelper.Install(clientRouter.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(5.0));

        Simulator::Stop(Seconds(6.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("TCP transmission test passed.");
    }

    // Test if the server receives TCP packets from the client
    void TestTcpReception() {
        NodeContainer clientRouter, routerServer;
        clientRouter.Create(2);
        routerServer.Add(clientRouter.Get(1));
        routerServer.Create(1);

        Address serverAddress(InetSocketAddress(Ipv4Address("10.1.2.2"), port));

        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", serverAddress);
        ApplicationContainer serverApp = sinkHelper.Install(routerServer.Get(1));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(6.0));

        Simulator::Stop(Seconds(6.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("TCP reception test passed.");
    }
};

// Instantiate the test suite
static TcpRoutedTestSuite tcpRoutedTestSuite;
