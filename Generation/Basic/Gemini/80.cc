#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h" // For TBF

// For DynamicCast to TokenBucketQueueDisc, we need its header
// Note: Direct access to private members like m_current0Tokens and m_current1Tokens
// is not possible from user code without modifying ns-3's source.
// The following include is mainly for type recognition in DynamicCast.
#include "ns3/token-bucket-queue-disc.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TokenBucketFilterSimulation");

// Global variables to hold command-line argument values
static double g_simTime = 10.0;             // Total simulation time in seconds
static DataRate g_tokenRate = DataRate("1Mbps"); // Token arrival rate (e.g., 1Mbps)
static DataRate g_peakRate = DataRate("2Mbps");  // Peak rate (e.g., 2Mbps)
static uint32_t g_bucket0Size = 10000;         // Size of the first token bucket in bytes
static uint32_t g_bucket1Size = 20000;         // Size of the second token bucket in bytes
static double g_tracingInterval = 0.5;        // Interval for logging TBF parameters in seconds

/**
 * \brief Helper class to periodically log TBF queue disc information.
 *
 * This class demonstrates how to set up periodic logging for a QueueDisc.
 * It also highlights the current limitation in ns-3 API regarding direct tracing
 * of internal dynamic state like m_current0Tokens and m_current1Tokens of
 * TokenBucketQueueDisc without modifying ns-3's source code.
 */
class TokenTracer
{
public:
    /**
     * \brief Constructor for TokenTracer.
     * \param qd The QueueDisc to trace.
     * \param interval The tracing interval in seconds.
     */
    TokenTracer(Ptr<QueueDisc> qd, double interval)
        : m_qd(qd), m_interval(interval)
    {
    }

    /**
     * \brief Method to be scheduled periodically to print token counts.
     *
     * This method attempts to dynamic_cast the QueueDisc to TokenBucketQueueDisc
     * and print its attributes. It notes the limitation of not being able to
     * access private internal token counts directly.
     */
    void Trace()
    {
        Ptr<TokenBucketQueueDisc> tbfQd = DynamicCast<TokenBucketQueueDisc>(m_qd);
        if (!tbfQd)
        {
            NS_LOG_WARN("Time " << Simulator::Now().GetSeconds() << "s: "
                                << "QueueDisc is not a TokenBucketQueueDisc. Cannot trace tokens.");
            return;
        }

        // --- IMPORTANT NOTE ON TRACING TOKEN COUNTS ---
        // As of ns-3.41, the internal dynamic token counts (m_current0Tokens,
        // m_current1Tokens) of ns3::TokenBucketQueueDisc are private members
        // without public getters or TracedValue exposure.
        // Therefore, they cannot be directly accessed and "traced" from user
        // application code (like this main.cc file) without modifying ns-3's
        // source code (e.g., adding public GetCurrent0Tokens()/GetCurrent1Tokens()
        // methods or making them TracedValue members).
        //
        // The following logging demonstrates *where* one would attempt to
        // print these values if they were publicly accessible.
        // For demonstration, we print *configured* TBF parameters, which are
        // accessible via GetAttribute, and explicitly state the limitation for dynamic counts.

        uint32_t bucket0Size = 0;
        uint32_t bucket1Size = 0;
        DataRate tokenRate = DataRate("0bps");
        DataRate peakRate = DataRate("0bps");

        // These are static configured values, not dynamic token counts
        tbfQd->GetAttribute("Bucket0Size", UintegerValue(bucket0Size));
        tbfQd->GetAttribute("Bucket1Size", UintegerValue(bucket1Size));
        tbfQd->GetAttribute("TokenRate", DataRateValue(tokenRate));
        tbfQd->GetAttribute("PeakRate", DataRateValue(peakRate));

        NS_LOG_INFO("Time " << Simulator::Now().GetSeconds() << "s: "
                            << "TBF Config (via GetAttribute) - Bucket0Size=" << bucket0Size << "B, "
                            << "Bucket1Size=" << bucket1Size << "B, "
                            << "TokenRate=" << tokenRate << ", PeakRate=" << peakRate << ". "
                            << "(Dynamic token counts not publicly exposed for direct tracing.)");

        // Schedule the next trace call
        if (Simulator::Now().GetSeconds() + m_interval < g_simTime)
        {
            Simulator::Schedule(Seconds(m_interval), &TokenTracer::Trace, this);
        }
    }

private:
    Ptr<QueueDisc> m_qd;
    double m_interval;
};


int main(int argc, char *argv[])
{
    // Enable logging for this component and potentially for the TokenBucketQueueDisc itself
    LogComponentEnable("TokenBucketFilterSimulation", LOG_LEVEL_INFO);
    // LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);    // Optional: for OnOff app output
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);         // Optional: for PacketSink output
    // LogComponentEnable("TokenBucketQueueDisc", LOG_LEVEL_INFO); // Enable internal TBF logs (not "tracing" as per request)

    // Parse command-line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Total simulation time in seconds", g_simTime);
    cmd.AddValue("tokenRate", "Token arrival rate (e.g., 1Mbps)", g_tokenRate);
    cmd.AddValue("peakRate", "Peak rate (e.g., 2Mbps)", g_peakRate);
    cmd.AddValue("bucket0Size", "Size of the first token bucket in bytes", g_bucket0Size);
    cmd.AddValue("bucket1Size", "Size of the second token bucket in bytes", g_bucket1Size);
    cmd.AddValue("tracingInterval", "Interval for logging TBF parameters in seconds", g_tracingInterval);
    cmd.Parse(argc, argv);

    // 1. Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // 2. Set up point-to-point link attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = p2p.Install(nodes);

    // 3. Install Internet stack on nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 4. Configure and install Token Bucket Filter (TBF) queue discipline
    // Apply TBF on the sender's device (Node 0's device, devices.Get(0))
    TrafficControlHelper tch;
    tch.Set
        ("QueueDiscFactory",
         "ns3::TokenBucketQueueDisc",
         "TokenRate", DataRateValue(g_tokenRate),
         "PeakRate", DataRateValue(g_peakRate),
         "Bucket0Size", UintegerValue(g_bucket0Size),
         "Bucket1Size", UintegerValue(g_bucket1Size));

    // Install the TBF queue discipline on the first device (Node 0's Tx device)
    Ptr<PointToPointNetDevice> senderDevice = DynamicCast<PointToPointNetDevice>(devices.Get(0));
    tch.Install(senderDevice);

    // Get the installed QueueDisc pointer for tracing and stats output
    Ptr<QueueDisc> senderQueueDisc = tch.GetQueueDisc(senderDevice);
    NS_ASSERT_MSG(senderQueueDisc != nullptr, "Failed to get QueueDisc from sender device.");
    NS_ASSERT_MSG(DynamicCast<TokenBucketQueueDisc>(senderQueueDisc) != nullptr, "Installed QueueDisc is not TokenBucketQueueDisc.");

    // 5. Setup OnOff traffic generator (Node 0 to Node 1)
    // Configure remote address and port for the OnOff application
    uint16_t port = 9; // Discard port
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));

    // Install PacketSink application on Node 1 (receiver)
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(g_simTime));

    // Install OnOff application on Node 0 (sender)
    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(1000)); // 1000 bytes per packet
    onoff.SetAttribute("DataRate", StringValue("5Mbps")); // Traffic rate (higher than link to ensure queuing)

    ApplicationContainer clientApps = onoff.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));         // Start traffic after 1 second
    clientApps.Stop(Seconds(g_simTime - 0.1)); // Stop traffic slightly before simulation ends

    // 6. Implement tracing for TBF (using the TokenTracer class defined above)
    // Create an instance of TokenTracer and schedule its first call.
    // The TokenTracer will then reschedule itself periodically.
    TokenTracer tracer(senderQueueDisc, g_tracingInterval);
    Simulator::Schedule(Seconds(g_tracingInterval), &TokenTracer::Trace, &tracer);

    // 7. Run simulation
    Simulator::Stop(Seconds(g_simTime));
    Simulator::Run();

    // 8. Output TBF queue statistics at the end of the simulation
    NS_LOG_INFO("--- TBF Queue Statistics (Node 0's Tx device) ---");
    senderQueueDisc->TraceQueueDiscStats(); // This prints aggregated stats to NS_LOG_INFO
    // Optionally, use the TrafficControlHelper to print more detailed stats
    tch.PrintQueueDiscStats(senderQueueDisc);

    Simulator::Destroy();
    return 0;
}