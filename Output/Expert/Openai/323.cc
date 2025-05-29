#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 8080;

    Address bindAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", bindAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("8.192Mbps"))); // 1024B/0.01s = 102.4KBps = 819.2kbps, but OnOff needs a higher rate to avoid bottleneck

    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Limit total number of packets via custom Application
    class PacketCounter : public Application
    {
    public:
        PacketCounter(Ptr<Application> app, uint32_t maxPackets, Time interval)
            : m_app(app), m_maxPackets(maxPackets), m_interval(interval), m_sent(0) {}

        void StartApplication() override
        {
            ScheduleNext();
        }
        void ScheduleNext()
        {
            if (m_sent < m_maxPackets)
            {
                m_app->GetObject<OnOffApplication>()->SetAttribute("StartTime", TimeValue(Simulator::Now()));
                m_app->GetObject<OnOffApplication>()->SetAttribute("StopTime", TimeValue(Simulator::Now() + m_interval));
                ++m_sent;
                Simulator::Schedule(m_interval, &PacketCounter::ScheduleNext, this);
            }
            else
            {
                m_app->SetStopTime(Simulator::Now());
            }
        }

    private:
        Ptr<Application> m_app;
        uint32_t m_maxPackets;
        Time m_interval;
        uint32_t m_sent;
    };

    Ptr<Application> onoffApp = clientApp.Get(0);
    Ptr<PacketCounter> controller = CreateObject<PacketCounter>(onoffApp, 1000, Seconds(0.01));
    nodes.Get(0)->AddApplication(controller);
    controller->SetStartTime(Seconds(2.0));
    controller->SetStopTime(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}