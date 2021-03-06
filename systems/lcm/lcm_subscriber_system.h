#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "drake/common/drake_copyable.h"
#include "drake/common/drake_deprecated.h"
#include "drake/common/drake_throw.h"
#include "drake/lcm/drake_lcm_interface.h"
#include "drake/systems/framework/basic_vector.h"
#include "drake/systems/framework/leaf_system.h"
#include "drake/systems/lcm/lcm_and_vector_base_translator.h"
#include "drake/systems/lcm/lcm_translator_dictionary.h"
#include "drake/systems/lcm/serializer.h"

namespace drake {
namespace systems {
namespace lcm {

/**
 * Receives LCM messages from a given channel and outputs them to a
 * System<double>'s port. This class stores the most recently processed LCM
 * message in the State. When a LCM message arrives asynchronously, an update
 * event is scheduled to process the message and store it in the State at the
 * earliest possible simulation time. The output is always consistent with the
 * State.
 *
 * To process a LCM message, CalcNextUpdateTime() needs to be called first to
 * check for new messages and schedule a callback event if a new LCM message
 * has arrived. The message is then processed and stored in the Context by
 * CalcUnrestrictedUpdate(). When this system is evaluated by the Simulator,
 * all these operations are taken care of by the Simulator. On the other hand,
 * the user needs to manually replicate this process without the Simulator.
 *
 * If LCM service in use is a drake::lcm::DrakeLcmLog (not live operation),
 * then see drake::systems::lcm::LcmLogPlaybackSystem for a helper to advance
 * the log cursor in concert with the simulation.
 *
 * @ingroup message_passing
 */
class LcmSubscriberSystem : public LeafSystem<double> {
 public:
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(LcmSubscriberSystem)

  /**
   * Factory method that returns a subscriber System that provides
   * Value<LcmMessage> message objects on its sole abstract-valued output port.
   *
   * @tparam LcmMessage message type to deserialize, e.g., lcmt_drake_signal.
   *
   * @param[in] channel The LCM channel on which to subscribe.
   *
   * @param lcm A non-null pointer to the LCM subsystem to subscribe on.
   */
  template <typename LcmMessage>
  static std::unique_ptr<LcmSubscriberSystem> Make(
      const std::string& channel, drake::lcm::DrakeLcmInterface* lcm) {
    return std::make_unique<LcmSubscriberSystem>(
        channel, std::make_unique<Serializer<LcmMessage>>(), lcm);
  }

  /**
   * (Experimental.) Factory method like Make(channel, lcm), but the result
   * only accepts bounded-size LCM messages.  The subscriber returned by this
   * method may perform better than the subscriber returned by a plain Make.
   * (To avoid issue #10149, this fixed-size subscriber will store the message
   * as discrete-state bytes, instead of a deserialized abstract value.)  Once
   * #10149 is resolved, this method might evaporate without a preceding
   * deprecation period.
   *
   * @param exemplar A sample message value; all messages received by this
   * System must be no larger than this encoded size.
   */
  template <typename LcmMessage>
  static std::unique_ptr<LcmSubscriberSystem> MakeFixedSize(
      const LcmMessage& exemplar, const std::string& channel,
      drake::lcm::DrakeLcmInterface* lcm) {
    // We can't use make_unique when calling a private constructor.
    return std::unique_ptr<LcmSubscriberSystem>(new LcmSubscriberSystem(
        channel, nullptr, std::make_unique<Serializer<LcmMessage>>(), lcm,
        exemplar.getEncodedSize()));
  }

  /**
   * Constructor that returns a subscriber System that provides message objects
   * on its sole abstract-valued output port.  The type of the message object is
   * determined by the @p serializer.
   *
   * @param[in] channel The LCM channel on which to subscribe.
   *
   * @param[in] serializer The serializer that converts between byte vectors
   * and LCM message objects.
   *
   * @param lcm A non-null pointer to the LCM subsystem to subscribe on.
   */
  LcmSubscriberSystem(const std::string& channel,
                      std::unique_ptr<SerializerInterface> serializer,
                      drake::lcm::DrakeLcmInterface* lcm);

  DRAKE_DEPRECATED("2019-05-01",
      "The LcmAndVectorBaseTranslator and its related code "
      "are scheduled to be removed, with no replacement.")
  LcmSubscriberSystem(const std::string&, const LcmAndVectorBaseTranslator&,
                      drake::lcm::DrakeLcmInterface*);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  DRAKE_DEPRECATED("2019-05-01",
      "The LcmAndVectorBaseTranslator and its related code "
      "are scheduled to be removed, with no replacement.")
  LcmSubscriberSystem(const std::string&, const LcmTranslatorDictionary&,
                      drake::lcm::DrakeLcmInterface*);
#pragma GCC diagnostic pop

  ~LcmSubscriberSystem() override;

  /// Returns the default name for a system that subscribes to @p channel.
  static std::string make_name(const std::string& channel);

  const std::string& get_channel_name() const;

  DRAKE_DEPRECATED("2019-05-01",
      "The LcmAndVectorBaseTranslator and its related code "
      "are scheduled to be removed, with no replacement.")
  const LcmAndVectorBaseTranslator& get_translator() const;

  /// Returns the sole output port.
  const OutputPort<double>& get_output_port() const {
    DRAKE_THROW_UNLESS(this->num_output_ports() == 1);
    return LeafSystem<double>::get_output_port(0);
  }

  // Don't use the indexed overload; use the no-arg overload.
  void get_output_port(int index) = delete;

  // This system has no input ports.
  void get_input_port(int) = delete;

  /**
   * Blocks the caller until its internal message count exceeds
   * `old_message_count` with an optional timeout.
   * @param old_message_count Internal message counter.
   *
   * @param message If non-null, will return the received message.
   *
   * @param timeout The duration (in seconds) to wait before returning; a
   * non-positive duration will not time out.
   *
   * @return Returns the new count of received messages. If a timeout occurred,
   * this will be less than or equal to old_message_count.
   *
   * @pre If `message` is specified, this system must be abstract-valued.
   */
  int WaitForMessage(int old_message_count, AbstractValue* message = nullptr,
                     double timeout = -1.) const;

  /**
   * (Advanced.) Writes the most recently received message (and message count)
   * into @p state.  If no messages have been received, only the message count
   * is updated.  This is primarily useful for unit testing.
   */
  DRAKE_DEPRECATED("2019-06-01",
      "This unit-test-only method is being made non-public.")
  void CopyLatestMessageInto(State<double>* state) const;

  /**
   * Returns the internal message counter. Meant to be used with
   * `WaitForMessage`.
   */
  int GetInternalMessageCount() const;

  /**
   * Returns the message counter stored in @p context.
   */
  int GetMessageCount(const Context<double>& context) const;

 protected:
  void DoCalcNextUpdateTime(const Context<double>& context,
                            systems::CompositeEventCollection<double>* events,
                            double* time) const override;

  void DoCalcUnrestrictedUpdate(
      const Context<double>&,
      const std::vector<const systems::UnrestrictedUpdateEvent<double>*>&,
      State<double>* state) const override {
    ProcessMessageAndStoreToAbstractState(&state->get_mutable_abstract_state());
  }

  void DoCalcDiscreteVariableUpdates(
      const Context<double>&,
      const std::vector<const systems::DiscreteUpdateEvent<double>*>&,
      DiscreteValues<double>* discrete_state) const override {
    ProcessMessageAndStoreToDiscreteState(discrete_state);
  }

 private:
  // All constructors delegate to here.
  LcmSubscriberSystem(const std::string& channel,
                      const LcmAndVectorBaseTranslator* translator,
                      std::unique_ptr<SerializerInterface> serializer,
                      drake::lcm::DrakeLcmInterface* lcm,
                      int fixed_encoded_size = -1);

  void ProcessMessageAndStoreToDiscreteState(
      DiscreteValues<double>* discrete_state) const;

  void ProcessMessageAndStoreToAbstractState(
      AbstractValues* abstract_state) const;

  // Callback entry point from LCM into this class.
  void HandleMessage(const void*, int);

  // This pair of methods is used for the output port when we're using a
  // translator.
  std::unique_ptr<BasicVector<double>> AllocateTranslatorOutputValue() const;
  void CalcTranslatorOutputValue(const Context<double>& context,
                                 BasicVector<double>* output_vector) const;

  // This pair of methods is used for the output port when we're using a
  // serializer.
  std::unique_ptr<AbstractValue> AllocateSerializerOutputValue() const;
  void CalcSerializerOutputValue(const Context<double>& context,
                                 AbstractValue* output_value) const;

  bool is_abstract_state() const {
    return serializer_ && (fixed_encoded_size_ < 0);
  }

  bool is_discrete_state() const {
    return !is_abstract_state();
  }

  // The channel on which to receive LCM messages.
  const std::string channel_;

  // Converts LCM message bytes to VectorBase objects.
  // Will be non-null iff our output port is vector-valued.
  const LcmAndVectorBaseTranslator* const translator_{};

  // Converts LCM message bytes to Value<LcmMessage> objects.
  // Will be non-null iff our output port is abstract-valued.
  const std::unique_ptr<SerializerInterface> serializer_;
  const int fixed_encoded_size_;

  // The mutex that guards received_message_ and received_message_count_.
  mutable std::mutex received_message_mutex_;

  // A condition variable that's signaled every time the handler is called.
  mutable std::condition_variable received_message_condition_variable_;

  // The bytes of the most recently received LCM message.
  std::vector<uint8_t> received_message_;

  // A message counter that's incremented every time the handler is called.
  int received_message_count_{0};

  // When we are destroyed, our subscription will be automatically removed
  // (if the DrakeLcmInterface supports removal).
  std::shared_ptr<drake::lcm::DrakeSubscriptionInterface> subscription_;

  // A little hint to help catch use-after-free.
  int magic_number_{};
};

}  // namespace lcm
}  // namespace systems
}  // namespace drake
