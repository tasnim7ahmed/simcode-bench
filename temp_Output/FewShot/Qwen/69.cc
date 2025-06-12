#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellTopologySimulation");

class DumbbellHelper {
public:
    DumbbellHelper(uint32_t nLeaf, std::string queueType, uint32_t minTh, uint32_t maxTh, double maxP, uint32_t pktSize,
                   DataRate rate, Time delay, uint32_t queueLimit)
        : m_nLeaf(nLeaf), m_queueType(queueType), m_minTh(minTh), m_maxTh(maxTh),
          m_maxP(maxP), m_pktSize(pktSize), m_rate(rate), m_delay(delay), m_queueLimit(queueLimit) {}

    void Install() {
        // Create left and right routers
        NodeContainer routers;
        routers.Create(2);

        // Create leaf nodes on each side
        NodeContainer leftLeafNodes, rightLeafNodes;
        leftLeafNodes.Create(m_nLeaf);
        rightLeafNodes.Create(m_nLeaf);

        // Connect each left leaf to left router
        NetDeviceContainer leftDevices;
        PointToPointHelper p2pLeft;
        p2pLeft.SetDeviceAttribute("DataRate", DataRateValue(m_rate));
        p2pLeft.SetChannelAttribute("Delay", TimeValue(m_delay));
        for (uint32_t i = 0; i < m_nLeaf; ++i) {
            NodeContainer leftPair(leftLeafNodes.Get(i), routers.Get(0));
            leftDevices.Add(p2pLeft.Install(leftPair));
        }

        // Connect each right leaf to right router
        NetDeviceContainer rightDevices;
        PointToPointHelper p2pRight;
        p2pRight.SetDeviceAttribute("DataRate", DataRateValue(m_rate));
        p2pRight.SetChannelAttribute("Delay", TimeValue(m_delay));
        for (uint32_t i = 0; i < m_nLeaf; ++i) {
            NodeContainer rightPair(routers.Get(1), rightLeafNodes.Get(i));
            rightDevices.Add(p2pRight.Install(rightPair));
        }

        // Connect the two routers
        NodeContainer routerPair(routers.Get(0), routers.Get(1));
        PointToPointHelper p2pRouter;
        p2pRouter.SetDeviceAttribute("DataRate", DataRateValue(m_rate));
        p2pRouter.SetChannelAttribute("Delay", TimeValue(m_delay));

        // Set queue discipline
        TrafficControlHelper tch;
        if (m_queueType == "red") {
            tch.SetRootQueueDisc("ns3::RedQueueDisc",
                                 "MinTh", UintegerValue(m_minTh),
                                 "MaxTh", UintegerValue(m_maxTh),
                                 "MaxP", DoubleValue(m_maxP),
                                 "QueueLimit", UintegerValue(m_queueLimit));
        } else if (m_queueType == "ared") {
            tch.SetRootQueueDisc("ns3::RedQueueDisc",
                                 "MinTh", UintegerValue(0), // adaptive MinTh
                                 "MaxTh", UintegerValue(m_maxTh),
                                 "MaxP", DoubleValue(m_maxP),
                                 "UseAdaptiveRed", BooleanValue(true),
                                 "QueueLimit", UintegerValue(m_queueLimit));
        }
        NetDeviceContainer routerLink = p2pRouter.Install(routerPair);
        tch.Install(routerLink);

        // Install internet stack
        InternetStackHelper stack;
        stack.InstallAll();

        // Assign IP addresses
        Ipv4AddressHelper ip;
        ip.SetBase("10.1.1.0", "255.255.255.0");
        ip.Assign(leftDevices);
        ip.NewNetwork();
        ip.SetBase("10.1.2.0", "255.255.255.0");
        ip.Assign(rightDevices);
        ip.NewNetwork();
        ip.SetBase("10.1.3.0", "255.255.255.0");
        Ipv4InterfaceContainer routerIfaces = ip.Assign(routerLink);

        // Setup TCP OnOff applications from right leaves to left leaves
        for (uint32_t i = 0; i < m_nLeaf; ++i) {
            Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), 80));
            PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
            ApplicationContainer sinkApp = sinkHelper.Install(leftLeafNodes.Get(i));
            sinkApp.Start(Seconds(0.0));
            sinkApp.Stop(Seconds(10.0));

            OnOffHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(routerIfaces.GetAddress(1), 80));
            clientHelper.SetAttribute("OnTime", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1.0]"));
            clientHelper.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1.0]"));
            clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
            clientHelper.SetAttribute("PacketSize", UintegerValue(m_pktSize));
            clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // infinite

            ApplicationContainer clientApp = clientHelper.Install(rightLeafNodes.Get(i));
            clientApp.Start(Seconds(1.0));
            clientApp.Stop(Seconds(10.0));
        }
    }

    void PrintStats() {
        QueueDiscContainer qdiscs = TrafficControlHelper::GetQueueDiscs(routerLink.Get(1));
        for (auto i = qdiscs.Begin(); i != qdiscs.End(); ++i) {
            (*i)->GetStats();
        }
    }

private:
    uint32_t m_nLeaf;
    std::string m_queueType;
    uint32_t m_minTh;
    uint32_t m_maxTh;
    double m_maxP;
    uint32_t m_pktSize;
    DataRate m_rate;
    Time m_delay;
    uint32_t m_queueLimit;
    NetDeviceContainer routerLink;
};

int main(int argc, char *argv[]) {
    uint32_t nLeaf = 4;
    std::string queueType = "red";
    uint32_t minTh = 5;
    uint32_t maxTh = 15;
    double maxP = 0.01;
    uint32_t pktSize = 1000;
    DataRate rate("1.5Mbps");
    Time delay = MilliSeconds(20);
    uint32_t queueLimit = 25;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nLeaf", "Number of leaf nodes on each side", nLeaf);
    cmd.AddValue("queueType", "Queue type: red or ared", queueType);
    cmd.AddValue("minTh", "RED minimum threshold (packets)", minTh);
    cmd.AddValue("maxTh", "RED maximum threshold (packets)", maxTh);
    cmd.AddValue("maxP", "RED maximum drop probability", maxP);
    cmd.AddValue("pktSize", "Packet size in bytes", pktSize);
    cmd.AddValue("rate", "Link data rate", rate);
    cmd.AddValue("delay", "Link delay", delay);
    cmd.AddValue("queueLimit", "Queue limit in packets", queueLimit);
    cmd.Parse(argc, argv);

    DumbbellHelper dumbbell(nLeaf, queueType, minTh, maxTh, maxP, pktSize, rate, delay, queueLimit);
    dumbbell.Install();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    dumbbell.PrintStats();
    Simulator::Destroy();

    return 0;
}